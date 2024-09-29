/* Edge Impulse ingestion SDK
 * Copyright (c) 2023 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
/**
 * @brief   Sample & upload data to Edge Impulse Studio.
 * @details Select 1 or multiple sensors by un-commenting the defines and select
 * a desired sample frequency. When this sketch runs, you can see raw sample
 * values outputted over the serial line. Now connect to the studio using the
 * `edge-impulse-data-forwarder` and start capturing data
 */
// #define SAMPLE_ACCELEROMETER

/**
 * Configure the sample frequency. This is the frequency used to send the data
 * to the studio regardless of the frequency used to sample the data from the
 * sensor. This differs per sensors, and can be modified in the API of the sensor
 */
#define FREQUENCY_HZ        10


/* Include ----------------------------------------------------------------- */
#include "Particle.h"
#include "ADXL362DMA.h"
#include "adxl345.h"

SYSTEM_THREAD(ENABLED);
SerialLogHandler logHandler(LOG_LEVEL_ERROR);

/* Constants --------------------------------------------------------------- */
#if (FREQUENCY_HZ <= 0)
#error "FREQUENCY_HZ should have a value greater dan 0"
#endif
#define INTERVAL_MS         (1000 / FREQUENCY_HZ)
#define CONVERT_G_TO_MS2    9.80665f

/* Forward declerations ---------------------------------------------------- */
void ei_printf(const char *format, ...);

/* Private variables ------------------------------------------------------- */
#ifdef SAMPLE_ACCELEROMETER
ADXL362DMA accel(SPI, A2);
#else
ADXL345 accel;
#endif

void setup()
{
    delay(2000);
    ei_printf("Edge Impulse sensor data ingestion\r\n");

    /* Init & start sensors */
#ifdef SAMPLE_ACCELEROMETER
    accel.softReset();
    while(accel.readStatus() == 0) {
        ei_printf("no status yet, waiting for accelerometer\r\n");
        delay(1000);
    }

    accel.writeFilterControl(accel.RANGE_2G, false, false, accel.ODR_200);
    accel.setMeasureMode(true);
#else
    accel.powerOn();
    accel.setRangeSetting(2);
#endif

}

void loop() {

    delay(INTERVAL_MS);

#ifdef SAMPLE_ACCELEROMETER
    int16_t acc[3];
    accel.readXYZ(acc[0], acc[1], acc[2]);
    ei_printf("%f, %f, %f,"
        ,(((float)(acc[0] * 2)) / 2048.f) * CONVERT_G_TO_MS2
        ,(((float)(acc[1] * 2)) / 2048.f) * CONVERT_G_TO_MS2
        ,(((float)(acc[2] * 2)) / 2048.f) * CONVERT_G_TO_MS2
    );
#else
    signed short xyz[3];
    accel.readAccel(xyz);
    ei_printf("%f, %f, %f,"
        ,(((float)xyz[0]) * 0.00389f) * CONVERT_G_TO_MS2
        ,(((float)xyz[1]) * 0.00389f) * CONVERT_G_TO_MS2
        ,(((float)xyz[2]) * 0.00389f) * CONVERT_G_TO_MS2
    );
#endif

    ei_printf("\r\n");
}

/**
* @brief      Printf function uses vsnprintf and output using Arduino Serial
*
* @param[in]  format     Variable argument list
*/
void ei_printf(const char *format, ...)
{
    static char print_buf[1024] = { 0 };

    va_list args;
    va_start(args, format);
    int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    if (r > 0) {
        Serial.write(print_buf);
    }
}
