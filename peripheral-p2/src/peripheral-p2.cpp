#include "Particle.h"
#include <math.h>
#include "Air_Quality_Sensor.h"
#include "SeeedOLED.h"

#define ENABLE_PIR 0
#define ENABLE_MAG 0
#define ENABLE_MIC 1

#define AQS_PIN D2
#define MIC_PIN A0
#define PIR_MAG_PIN D5
#define DEVICE_SER_ID 2

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

uint32_t lastUpdate = 0;

SerialLogHandler logHandler(LOG_LEVEL_INFO);
AirQualitySensor aqSensor(AQS_PIN);
BleAdvertisingData *data;

enum class DeviceReportType : uint8_t
{
  PIR = 0,
  SOUND_PEAK,
  MAGNET,
  AIR_QUALITY
};

enum class AQSReportType : uint8_t
{
  NONE = 0,
  FRESH,
  LOW,
  HIGH,
  DANGER
};

struct DeviceReportEntry
{
  DeviceReportType type;
  uint8_t value;
};

struct DeviceReport
{
  uint8_t deviceId : 3;
  uint8_t valueLen : 5;
  DeviceReportEntry values[4];
};

void updateAdvData(void);
void updateOled(DeviceReport *);
AQSReportType getAirQuality(void);
const char *getAqsString(AQSReportType);

void setup()
{
#if ENABLE_PIR || ENABLE_MAG
  pinMode(PIR_MAG_PIN, INPUT);
#endif

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

  BLE.setAdvertisingInterval(800);
}

void loop()
{
  if (millis() - lastUpdate >= 2400)
  {
    lastUpdate = millis();
    updateAdvData();
  }
}

void updateAdvData()
{
  BLE.stopAdvertising();

  if (data)
  {
    delete data;
  }

  uint8_t valueCount = ENABLE_MAG + ENABLE_PIR + ENABLE_MIC;

  DeviceReport reportData;
  reportData.deviceId = DEVICE_SER_ID;
  reportData.valueLen = valueCount + 1;

  reportData.values[0] = {
      .type = DeviceReportType::AIR_QUALITY,
      .value = static_cast<uint8_t>(getAirQuality())};

#if ENABLE_PIR
  reportData.values[1] = {
      .type = DeviceReportType::PIR,
      .value = static_cast<uint8_t>(digitalRead(PIR_MAG_PIN)),
  };
#endif

#if ENABLE_MAG
  reportData.values[1 + ENABLE_PIR] = {
      .type = DeviceReportType::MAGNET,
      .value = static_cast<uint8_t>(digitalRead(PIR_MAG_PIN)),
  };
#endif

#if ENABLE_MIC
  reportData.values[1 + ENABLE_PIR + ENABLE_MAG] = {
      .type = DeviceReportType::SOUND_PEAK,
      .value = static_cast<uint8_t>(analogRead(MIC_PIN) >= 2048),
  };
#endif

  updateOled(&reportData);

  data = new BleAdvertisingData();
  data->appendLocalName("SEC_MONITOR");
  data->append(BleAdvertisingDataType::MANUFACTURER_SPECIFIC_DATA, reinterpret_cast<uint8_t *>(&reportData), sizeof(reportData));
  BLE.setAdvertisingInterval(800);
  BLE.advertise(data);
}

void updateOled(DeviceReport *report)
{
  char buf[32];
  buf[31] = '\0';

  for (auto i = 0; i < report->valueLen; i++)
  {
    auto &entry = report->values[i];
    switch (entry.type)
    {
    case DeviceReportType::AIR_QUALITY:
    {
      snprintf(buf, 32, "AQS: %s", getAqsString(static_cast<AQSReportType>(entry.value)));
      break;
    }

    case DeviceReportType::PIR:
    {
      snprintf(buf, 32, "PIR: %s", entry.value ? "Detected" : "None");
      break;
    }

    case DeviceReportType::MAGNET:
    {
      snprintf(buf, 32, "MAGNET: %s", entry.value ? "Closed" : "Open");
      break;
    }
    case DeviceReportType::SOUND_PEAK:
    {
      snprintf(buf, 32, "SOUND: %s", entry.value ? "Above" : "Below");
      break;
    }
    }

    SeeedOled.setTextXY(i, 0);
    SeeedOled.putString(buf);
  }
}

AQSReportType getAirQuality()
{
  int quality = aqSensor.slope();

  if (quality == AirQualitySensor::FORCE_SIGNAL)
  {
    return AQSReportType::DANGER;
  }

  if (quality == AirQualitySensor::HIGH_POLLUTION)
  {
    return AQSReportType::HIGH;
  }

  if (quality == AirQualitySensor::LOW_POLLUTION)
  {
    return AQSReportType::LOW;
  }

  if (quality == AirQualitySensor::FRESH_AIR)
  {
    return AQSReportType::FRESH;
  }

  return AQSReportType::NONE;
}

const char *getAqsString(AQSReportType type)
{
  switch (type)
  {
  case AQSReportType::NONE:
    return "None";
  case AQSReportType::FRESH:
    return "Fresh";
  case AQSReportType::LOW:
    return "Low";
  case AQSReportType::HIGH:
    return "High";
  case AQSReportType::DANGER:
    return "Danger";
  default:
    return "Error";
  }
}