#define EIDSP_QUANTIZE_FILTERBANK 0
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 4

#include "Particle.h"
#include "Microphone_PDM.h"
#include <Break-in_Detection_inferencing.h>
#include <math.h>
#include "Air_Quality_Sensor.h"
#include "SeeedOLED.h"

#define AQS_PIN D2

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

static bool microphone_inference_start(uint32_t n_samples);
static bool microphone_inference_record(void);
static void microphone_inference_end(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);

AirQualitySensor aqSensor(AQS_PIN);

/** Audio buffers, pointers and selectors */
typedef struct
{
  signed short *buffers[2];
  unsigned char buf_select;
  unsigned char buf_ready;
  unsigned int buf_count;
  unsigned int n_samples;
} inference_t;

static inference_t inference;
static bool record_ready = false;
static signed short *sampleBuffer;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);

void setup()
{
  run_classifier_init();
  if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false)
  {
    ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    return;
  }

  if (aqSensor.init())
  {
    Serial.println("Air Quality Sensor ready.");
  }
  else
  {
    Serial.println("Air Quality Sensor ERROR!");
  }

  Wire.begin();
  SeeedOled.init();

  SeeedOled.clearDisplay();
  SeeedOled.setInverseDisplay();
  SeeedOled.setPageMode();

  SeeedOled.sendCommand(0xA8);
  SeeedOled.sendCommand(0x3F);
  SeeedOled.sendCommand(0xA0);
  SeeedOled.sendCommand(0xC9);
  SeeedOled.sendCommand(0xA1);
}

void loop()
{
  bool m = microphone_inference_record();
  if (!m)
  {
    ei_printf("ERR: Failed to record audio...\n");
    return;
  }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
  signal.get_data = &microphone_audio_signal_get_data;
  ei_impulse_result_t result = {0};

  EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
  if (r != EI_IMPULSE_OK)
  {
    ei_printf("ERR: Failed to run classifier (%d)\n", r);
    return;
  }

  if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW))
  {
    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
              result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
      ei_printf("    %s: %.5f\n", result.classification[ix].label,
                result.classification[ix].value);
    }

    print_results = 0;
  }
}

static int16_t *sptr;
static uint32_t sample_length = 0;

/**
 * @brief      PDM buffer full callback
 *             Get data and call audio thread callback
 */
static void pdm_data_ready_inference_callback(void)
{
  bool dma_ready = Microphone_PDM::instance().noCopySamples([](void *pSamples, size_t numSamples)
                                                            {

        sample_length = Microphone_PDM::instance().getBufferSizeInBytes() / 2;
        sptr = (int16_t *)pSamples; });

  if (record_ready == true && dma_ready)
  {

    for (int i = 0; i < sample_length; i++)
    {
      inference.buffers[inference.buf_select][inference.buf_count++] = sptr[i];

      if (inference.buf_count >= inference.n_samples)
      {
        inference.buf_select ^= 1;
        inference.buf_count = 0;
        inference.buf_ready = 1;
      }
    }
  }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
  inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

  if (inference.buffers[0] == NULL)
  {
    return false;
  }

  inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

  if (inference.buffers[1] == NULL)
  {
    free(inference.buffers[0]);
    return false;
  }

  sampleBuffer = (signed short *)malloc((n_samples >> 1) * sizeof(signed short));

  if (sampleBuffer == NULL)
  {
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    return false;
  }

  inference.buf_select = 0;
  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  int err = Microphone_PDM::instance()
                .withOutputSize(Microphone_PDM::OutputSize::SIGNED_16)
                .withRange(Microphone_PDM::Range::RANGE_32768)
                .withSampleRate(16000)
                .init();

  if (err)
  {
    ei_printf("PDM decoder init err=%d\r\n", err);
  }

  if (Microphone_PDM::instance().start())
  {
    ei_printf("Failed to start PDM!");
    microphone_inference_end();

    return false;
  }

  record_ready = true;

  return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
  bool ret = true;

  if (inference.buf_ready == 1)
  {
    ei_printf(
        "Error sample buffer overrun. Decrease the number of slices per model window "
        "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
    ret = false;
  }

  while (inference.buf_ready == 0)
  {
    pdm_data_ready_inference_callback();
  }

  inference.buf_ready = 0;

  return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
  numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

  return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
  Microphone_PDM::instance().stop();
  free(inference.buffers[0]);
  free(inference.buffers[1]);
  free(sampleBuffer);
}

String getAirQuality()
{
  int quality = aqSensor.slope();
  String qual = "None";

  if (quality == AirQualitySensor::FORCE_SIGNAL)
  {
    qual = "Danger";
  }
  else if (quality == AirQualitySensor::HIGH_POLLUTION)
  {
    qual = "High Pollution";
  }
  else if (quality == AirQualitySensor::LOW_POLLUTION)
  {
    qual = "Low Pollution";
  }
  else if (quality == AirQualitySensor::FRESH_AIR)
  {
    qual = "Fresh Air";
  }

  return qual;
}