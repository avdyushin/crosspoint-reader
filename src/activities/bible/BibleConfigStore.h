#pragma once

#include "BibleToolbox.h"
#include "PersistableStore.h"

class BibleConfigStore : public PersistableStore<BibleConfigStore> {
  static constexpr auto CONFIG_PATH = "/.bible/config.json";
  static constexpr int8_t CONFIG_VERSION = 1;
  mutable std::mutex configMutex;

 public:
  std::string version;
  std::string biblePath;
  int bookIndex;
  int chapterNumber = BibleToolbox::START_CHAPTER_NUMBER;
  int pageNumber;

  ~BibleConfigStore() { auto _ = saveToFile(); }

  void clear() {
    version = CONFIG_VERSION;
    biblePath.clear();
    bookIndex = 0;
    chapterNumber = BibleToolbox::START_CHAPTER_NUMBER;
    pageNumber = 0;
  }

  static const char* getFilePath() { return CONFIG_PATH; }

  void toJson(JsonDocument& doc) const {
    std::lock_guard lock(configMutex);
    doc["version"] = CONFIG_VERSION;
    doc["module"] = biblePath;
    doc["bookIndex"] = bookIndex;
    doc["chapterNumber"] = chapterNumber;
    doc["pageNumber"] = pageNumber;
  }

  bool fromJson(const JsonVariantConst doc) {
    std::lock_guard lock(configMutex);
    version = doc["version"] | CONFIG_VERSION;
    biblePath = doc["module"] | "";
    bookIndex = doc["bookIndex"] | 0;
    chapterNumber = doc["chapterNumber"] | BibleToolbox::START_CHAPTER_NUMBER;
    pageNumber = doc["pageNumber"] | 0;
    return true;
  }
};
