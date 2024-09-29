#include "Particle.h"
#include "location.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);
SerialLogHandler logHandler(LOG_LEVEL_INFO);
Ledger ledger;

void getLocationCb(void);
void locCb(LocationResults);
Timer timer(500, getLocationCb);

static uint32_t lastUpdateTs = 0;
static LocationPoint lastPos = {};
static bool hasFixedPos = false;

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

void logSecurityReport(DeviceReport *);

void setup()
{
  ledger = Particle.ledger("security-status");
  LocationConfiguration config;
  config.enableAntennaPower(GNSS_ANT_PWR);
  Location.begin(config);

  timer.start();

  DeviceReport initialReport = {
      .deviceId = 0,
      .valueLen = 4,
      .values = {
          {.type = DeviceReportType::AIR_QUALITY,
           .value = 0},
          {.type = DeviceReportType::PIR,
           .value = 0},
          {.type = DeviceReportType::MAGNET,
           .value = 1},
          {.type = DeviceReportType::SOUND_PEAK,
           .value = 0},
      }};

  logSecurityReport(&initialReport);
}

void loop()
{
  if (millis() - lastUpdateTs >= 3000)
  {
    lastUpdateTs = millis();

    BleScanFilter filter;
    filter.deviceName("SEC_MONITOR");
    auto results = BLE.scanWithFilter(filter);

    for (auto &result : results)
    {
      auto advData = result.advertisingData();
      DeviceReport rpt;
      advData.customData(reinterpret_cast<uint8_t *>(&rpt), sizeof(rpt));

      logSecurityReport(&rpt);
    }
  }
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

void logSecurityReport(DeviceReport *report)
{
  Variant variant;

  for (uint8_t i = 0; i < report->valueLen; i++)
  {
    auto &entry = report->values[i];

    if (entry.type == DeviceReportType::AIR_QUALITY)
    {
      variant["aqs"] = getAqsString(static_cast<AQSReportType>(entry.value));
    }
    else if (entry.type == DeviceReportType::PIR)
    {
      variant["pir"] = entry.value ? "Detected" : "None";
    }
    else if (entry.type == DeviceReportType::MAGNET)
    {
      variant["magnet"] = entry.value ? "Closed" : "Open";
    }
    else if (entry.type == DeviceReportType::SOUND_PEAK)
    {
      variant["sound"] = entry.value ? "Above" : "Below";
    }

    if (hasFixedPos)
    {
      Variant locVariant;
      locVariant["lat"] = lastPos.latitude;
      locVariant["lon"] = lastPos.longitude;

      variant["loc"] = locVariant;
    }

    Log.info("Setting ledger to %s", variant.toString().c_str());
    ledger.set(variant, Ledger::SetMode::MERGE);
  }
}

void locCb(LocationResults results)
{
  hasFixedPos = results == LocationResults::Fixed;
}

void getLocationCb()
{
  Location.getLocation(lastPos, locCb, false);
}
