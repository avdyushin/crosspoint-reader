#pragma once

#include "Constants.h"
#include "PersistableStore.h"

class ReadingPlanConfigStore : public PersistableStore<ReadingPlanConfigStore> {
  static constexpr auto CONFIG_PATH = "/.bible/reading.json";
  static constexpr int8_t CONFIG_VERSION = 1;
  mutable std::mutex configMutex;

 public:
  std::string version;
  std::string readingPlanPath;
  int readingPlanDay{BibleToolbox::START_READING_DAY};
  int pageNumber;

  ~ReadingPlanConfigStore() { auto _ = saveToFile(); }

  void clear() {
    version = CONFIG_VERSION;
    readingPlanPath.clear();
    readingPlanDay = BibleToolbox::START_READING_DAY;
    pageNumber = 0;
  }

  static const char* getFilePath() { return CONFIG_PATH; }

  void toJson(JsonDocument& doc) const {
    std::lock_guard lock(configMutex);
    doc["version"] = CONFIG_VERSION;
    doc["readingPlanPath"] = readingPlanPath;
    doc["readingPlanDay"] = readingPlanDay;
    doc["pageNumber"] = pageNumber;
  }

  bool fromJson(const JsonVariantConst doc) {
    std::lock_guard lock(configMutex);
    version = doc["version"] | CONFIG_VERSION;
    readingPlanPath = doc["readingPlanPath"] | "";
    readingPlanDay = doc["readingPlanDay"] | BibleToolbox::START_READING_DAY;
    pageNumber = doc["pageNumber"] | 0;
    return true;
  }
};
