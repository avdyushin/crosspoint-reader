#include "ReadingPlanActivity.h"

#include "sqlite3_hal.h"
#include "util/BibleVerseFormatter.h"

namespace {
constexpr auto MODULE_TAG = "READING_PLAN";
}

std::filesystem::path ReadingPlanActivity::getCacheDir() const {
  return std::filesystem::path("/") / ".bible" / std::string(bible_->id());
}

std::string ReadingPlanActivity::getCacheFileName() const { return getCacheDir() / "day.html"; }

std::string ReadingPlanActivity::getBinFileName() const {
  return getCacheDir() / std::format("day_{}.bin", chapterNavigator_.getChapter());
}

std::string ReadingPlanActivity::getChapterTitle() const {
  return std::format("{} {}", chapterNavigator_.currentBookName(), chapterNavigator_.getChapter());
}

std::string ReadingPlanActivity::getBookTitle() const { return std::string(bible_->description()); }

std::string ReadingPlanActivity::getLanguage() const { return std::string(bible_->language()); }

bool ReadingPlanActivity::isAtEndOfBook() const { return false; }

bool ReadingPlanActivity::loadBook() {
  if (bookPath.empty()) {
    bookPath = config_.readingPlanPath;
    LOG_INF(MODULE_TAG, "No module path provided, using last opened: %s", bookPath.c_str());
  }

  LOG_DBG(MODULE_TAG, "ReadingPlanActivity::loadBook '%s'", bookPath.c_str());
  readingPlan_ = std::make_unique<BibleToolbox::ReadingPlan>(bookPath, HAL_VFS_NAME);
  if (!readingPlan_) {
    LOG_ERR(MODULE_TAG, "ReadingPlanActivity::readingPlan_ is nullptr for '%s' using '%s' VFS", bookPath.c_str(),
            HAL_VFS_NAME);
    return false;
  }
  if (readingPlan_->daysCount() == 0) {
    LOG_INF(MODULE_TAG, "Empty Reading Plan");
    return false;
  }
  dailyNavigator_.configureWith("Daily Reading", readingPlan_->daysCount());
  if (!chapterNavigator_.setPosition(
          BibleToolbox::BookPosition{.book = 0, .chapter = config_.readingPlanDay, .page = config_.pageNumber})) {
    LOG_INF(MODULE_TAG, "Loaded book position is out of bounds!");
  }
  return loadChapter(TargetPage{config_.pageNumber});
}

bool ReadingPlanActivity::loadChapter(const StartPagePosition startPagePosition) {
  section_.reset();
  if (!Storage.exists(getCacheDir().c_str())) {
    Storage.mkdir(getCacheDir().c_str());
  }
  return layout(startPagePosition);
}

void ReadingPlanActivity::formatChapter(const serialization::BufferedFileWriterIterator iter) {
  constexpr auto formatter = BibleVerseFormatter{};
  formatter.readingDayVerses(iter, *readingPlan_, *bible_, chapterNavigator_.getChapter(), "Day");
}

void ReadingPlanActivity::onPositionChanged(const BibleToolbox::BookPosition oldPosition,
                                            const BibleToolbox::BookPosition newPosition,
                                            const BibleToolbox::PositionChange changes) {
  LOG_INF(MODULE_TAG, "Book position changed from %d:%d:%d to %d:%d:%d", oldPosition.book, oldPosition.chapter,
          oldPosition.page, newPosition.book, newPosition.chapter, newPosition.page);
  if (BibleToolbox::hasChange(changes, BibleToolbox::PositionChange::Page)) {
    config_.pageNumber = newPosition.page;
  }
  if (BibleToolbox::hasChange(changes, BibleToolbox::PositionChange::Chapter)) {
    config_.readingPlanDay = newPosition.chapter;
    StartPagePosition pageNavigation = TargetPage{newPosition.page};
    if (newPosition.chapter < oldPosition.chapter) {
      pageNavigation = LastPage{};
    }
    this->loadChapter(pageNavigation);
  }
}

void ReadingPlanActivity::loop() {
  ReaderActivity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    LOG_DBG(MODULE_TAG, "ReadingPlanActivity: Confirm");
  }
}

bool ReadingPlanActivity::handleBackNavigation() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  return true;
}
