#include "BibleActivity.h"

#include <ranges>

#include "BibleBookSelectionActivity.h"
#include "BufferedFileWriterIterator.h"
#include "CrossPointState.h"
#include "ReadingPlan.h"
#include "ReadingPlanActivity.h"
#include "RecentBooksStore.h"
#include "activities/home/FileBrowserActivity.h"
#include "sqlite3_hal.h"
#include "util/BibleVerseFormatter.h"

namespace {
constexpr auto MODULE_TAG = "BIBLE";
}

std::filesystem::path BibleActivity::getCacheDir() const {
  return std::filesystem::path("/") / ".bible" / std::string(bible_->id());
}

std::string BibleActivity::getCacheFileName() const { return getCacheDir() / "chapter.html"; }

std::string BibleActivity::getBinFileName() const {
  return getCacheDir() /
         std::format("{}_{}.bin", chapterNavigator_.currentBookNumber(), chapterNavigator_.getChapter());
}

std::string BibleActivity::getChapterTitle() const {
  return std::format("{} {}", chapterNavigator_.currentBookName(), chapterNavigator_.getChapter());
}

std::string BibleActivity::getBookTitle() const { return std::string(bible_->description()); }

std::string BibleActivity::getLanguage() const { return std::string(bible_->language()); }

bool BibleActivity::loadBook() {
  if (!config_.loadFromFile()) {
    LOG_INF(MODULE_TAG, "Could not load configuration file");
  }

  if (!readingPlanConfig_.loadFromFile()) {
    LOG_INF(MODULE_TAG, "Could not load reading plan configuration file");
  }

  if (bookPath.empty()) {
    bookPath = config_.biblePath;
    LOG_INF(MODULE_TAG, "No module path provided, using last opened: %s", bookPath.c_str());
  }

  bible_.reset();
  bible_ = std::make_shared<BibleToolbox::Bible>(bookPath, HAL_VFS_NAME);
  config_.biblePath = bookPath;  // Save loaded module

  if (!bible_->isValid()) {
    LOG_INF(MODULE_TAG, "Couldn't load Bible module (no books found)");
    bible_.reset();
    config_.clear();  // Reset unloadable module
    return false;
  }

  APP_STATE.openEpubPath = bookPath;
  auto _ = APP_STATE.saveToFile();

  bibleNavigator_.configureWith(bible_->books());
  if (!chapterNavigator_.setPosition(BibleToolbox::BookPosition{
          .book = config_.bookIndex, .chapter = config_.chapterNumber, .page = config_.pageNumber})) {
    LOG_INF(MODULE_TAG, "Loaded book position is out of bounds!");
  }

  return loadChapter(TargetPage{config_.pageNumber});
}

bool BibleActivity::loadChapter(const StartPagePosition startPagePosition) {
  section_.reset();

  // Update recents
  RECENT_BOOKS.addBook(bookPath, getBookTitle(), getChapterTitle(), "");

  if (!Storage.exists(getCacheDir().c_str())) {
    Storage.mkdir(getCacheDir().c_str());
  }

  return layout(startPagePosition);
}

void BibleActivity::formatChapter(const serialization::BufferedFileWriterIterator iter) {
  if (bible_) {
    constexpr auto formatter = BibleVerseFormatter{};
    formatter.formatChapter(iter, *bible_, chapterNavigator_.currentBookNumber(), chapterNavigator_.getChapter(),
                            bible_->chapterString());
  }
}

void BibleActivity::onPositionChanged(const BibleToolbox::BookPosition oldPosition,
                                      const BibleToolbox::BookPosition newPosition,
                                      const BibleToolbox::PositionChange changes) {
  LOG_INF(MODULE_TAG, "Book position changed from %d:%d:%d to %d:%d:%d", oldPosition.book, oldPosition.chapter,
          oldPosition.page, newPosition.book, newPosition.chapter, newPosition.page);
  if (BibleToolbox::hasChange(changes, BibleToolbox::PositionChange::Book)) {
    config_.bookIndex = newPosition.book;
  }
  if (BibleToolbox::hasChange(changes, BibleToolbox::PositionChange::Chapter)) {
    config_.chapterNumber = newPosition.chapter;
  }
  if (BibleToolbox::hasChange(changes, BibleToolbox::PositionChange::Page)) {
    config_.pageNumber = newPosition.page;
  }
  if (BibleToolbox::hasChange(changes, BibleToolbox::PositionChange::Book | BibleToolbox::PositionChange::Chapter)) {
    RECENT_BOOKS.updateBook(bookPath, getBookTitle(), getChapterTitle(), "");
    StartPagePosition pageNavigation = TargetPage{newPosition.page};
    if (newPosition.book < oldPosition.book || newPosition.chapter < oldPosition.chapter) {
      pageNavigation = LastPage{};
    }
    this->loadChapter(pageNavigation);
  }
}

void BibleActivity::loop() {
  ReaderActivity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto moduleId = bible_ == nullptr ? "" : std::string(bible_->id());
    const auto bookName = bible_ == nullptr ? "" : std::string(chapterNavigator_.currentBookName());
    const std::filesystem::path readingPlanPath = readingPlanConfig_.readingPlanPath;
    const auto readingPlanId = !readingPlanPath.has_filename() ? "" : readingPlanPath.stem().string();
    const BibleMenuActivity::Config config{.currentModuleId = moduleId,
                                           .currentBookName = bookName,
                                           .currentChapterNumber = chapterNavigator_.getChapter(),
                                           .readingPlanId = readingPlanId,
                                           .readingPlanDay = readingPlanConfig_.readingPlanDay};
    auto menu = std::make_unique<BibleMenuActivity>(renderer, mappedInput, config);
    auto handler = [this](const ActivityResult& result) {
      const auto& menuResult = std::get<MenuResult>(result.data);
      if (!result.isCancelled) {
        handleMenuAction(static_cast<BibleMenuActivity::MenuItem>(menuResult.action));
      }
      requestUpdate();
    };
    startActivityForResult(std::move(menu), handler);
  }
}

void BibleActivity::handleMenuAction(const BibleMenuActivity::MenuItem menuItem) {
  auto openReadingPlan = [this](const std::string& path) {
    auto activity =
        std::make_unique<ReadingPlanActivity>(bible_, readingPlanConfig_, renderer, mappedInput, path, false);
    auto handler = [this](const ActivityResult& result) {
      LOG_INF(MODULE_TAG, "Reading plan closed");
      requestUpdate();
    };
    startActivityForResult(std::move(activity), handler);
  };

  switch (menuItem) {
    case BibleMenuActivity::MODULE: {
      const std::filesystem::path modulePath{bookPath};
      const auto parent = modulePath.parent_path();
      auto browser =
          std::make_unique<FileBrowserActivity>(renderer, mappedInput, parent, FileBrowserActivity::Mode::Bibles);
      auto handler = [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& [path] = std::get<FilePathResult>(result.data);
          LOG_DBG(MODULE_TAG, "Selected module path = '%s'", path.c_str());
          bookPath = path;
          loadBook();
        }
        requestUpdate();
      };
      startActivityForResult(std::move(browser), handler);
      break;
    }
    case BibleMenuActivity::BOOK: {
      auto menu = std::make_unique<BibleBookSelectionActivity>(renderer, mappedInput, "Select Book", bible_->books(),
                                                               chapterNavigator_.getBook());
      auto handler = [this](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          const auto book = menuResult.action;
          LOG_INF(MODULE_TAG, "Selected Book index = %d", book);
          if (chapterNavigator_.setPosition(
                  BibleToolbox::BookPosition{.book = book, .chapter = BibleToolbox::START_CHAPTER_NUMBER, .page = 0})) {
            loadChapter(TargetPage{});
          } else {
            LOG_DBG(MODULE_TAG, "Active book selected or out of range: %d", book);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
    }
    case BibleMenuActivity::CHAPTER: {
      const auto chapterCount = chapterNavigator_.chapterCount();
      const auto chapterString = bible_->chapterString();
      chapterListCache_.clear();
      chapterListCache_.reserve(chapterCount);
      auto chapterView = std::views::iota(BibleToolbox::START_CHAPTER_NUMBER, chapterCount + 1) |
                         std::views::transform([chapterString](const int i) {
                           return BibleChapterInfo{.name = std::string(chapterString) + " " + std::to_string(i)};
                         });
      std::ranges::copy(chapterView, std::back_inserter(chapterListCache_));
      auto menu = std::make_unique<BibleChapterSelectionActivity>(
          renderer, mappedInput, "Select Chapter", chapterListCache_,
          chapterNavigator_.getChapter() - BibleToolbox::START_CHAPTER_NUMBER);
      auto handler = [this](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          if (const auto chapter = menuResult.action + BibleToolbox::START_CHAPTER_NUMBER;
              chapterNavigator_.setPosition({.book = chapterNavigator_.getBook(), .chapter = chapter, .page = 0})) {
            loadChapter(TargetPage{});
          } else {
            LOG_DBG(MODULE_TAG, "Active chapter selected or out of range: %d", chapter);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
    }
    case BibleMenuActivity::READING_PLAN: {
      const std::filesystem::path modulePath{bookPath};
      const auto parent = modulePath.parent_path();
      auto browser =
          std::make_unique<FileBrowserActivity>(renderer, mappedInput, parent, FileBrowserActivity::Mode::ReadingPlans);
      auto handler = [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& [path] = std::get<FilePathResult>(result.data);
          LOG_DBG(MODULE_TAG, "Selected reading plan path = '%s'", path.c_str());
          readingPlanConfig_.readingPlanPath = path;
        }
        requestUpdate();
      };
      startActivityForResult(std::move(browser), handler);
      break;
    }
    case BibleMenuActivity::DAILY_READING: {
      auto activity = std::make_unique<ReadingPlanActivity>(bible_, readingPlanConfig_, renderer, mappedInput,
                                                            readingPlanConfig_.readingPlanPath, false);
      auto handler = [this](const ActivityResult&) { requestUpdate(); };
      startActivityForResult(std::move(activity), handler);
      break;
    }
  }
}
