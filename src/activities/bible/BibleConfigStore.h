#pragma once
#include <filesystem>

#include "PersistableStore.h"

class BibleConfigStore : public PersistableStore<BibleConfigStore> {
  static constexpr auto CONFIG_PATH = "/.bible/config.json";
  static constexpr int8_t CONFIG_VERSION = 1;
  mutable std::mutex configMutex;

 public:
  struct Config {
    std::string version;
    std::string module;
    int bookIndex;
    int chapterNumber;
    int pageNumber;
  };

  Config config{};

  static const char* getFilePath() { return CONFIG_PATH; }

  void toJson(JsonDocument& doc) const {
    std::lock_guard lock(configMutex);
    doc["version"] = CONFIG_VERSION;
    doc["module"] = config.module;
    doc["bookIndex"] = config.bookIndex;
    doc["chapterNumber"] = config.chapterNumber;
    doc["pageNumber"] = config.pageNumber;
  }

  bool fromJson(JsonVariantConst doc) {
    std::lock_guard lock(configMutex);
    config.version = doc["version"] | CONFIG_VERSION;
    config.module = doc["module"] | "";
    config.bookIndex = doc["bookIndex"] | 0;
    config.chapterNumber = doc["chapterNumber"] | 1;
    config.pageNumber = doc["pageNumber"] | 0;
    return true;
  }
};
