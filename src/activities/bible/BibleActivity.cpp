#include "BibleActivity.h"

#include <Bible.h>
#include <sys/stat.h>

#include <filesystem>
#include <ranges>

#include "../reader/ReaderUtils.h"
#include "BibleBookSelectionActivity.h"
#include "BibleChapterSelectionActivity.h"
#include "BibleConfigStore.h"
#include "BibleMenuActivity.h"
#include "BufferedFile.h"
#include "BufferedFileWriterIterator.h"
#include "CrossPointState.h"
#include "Epub/Page.h"
#include "Epub/parsers/ChapterHtmlSlimParser.h"
#include "FontCacheManager.h"
#include "I18n.h"
#include "I18nKeys.h"
#include "RecentBooksStore.h"
#include "SdCardFontSystem.h"
#include "activities/home/FileBrowserActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "sqlite3_hal.h"
#include "util/BibleVerseFormatter.h"

namespace {

constexpr auto MODULE_TAG = "BIBLE";
constexpr size_t IO_BUFFER_SIZE = 4096;  // 4k in sync with BUILD_IO_BUFFER_SIZE
constexpr auto BUILD_PAGES_PER_CHUNK = 8;

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

}  // namespace

BibleActivity::BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                             const bool allowFastInitialRefresh)
    : ReaderActivity("BibleActivity", renderer, mappedInput, std::move(bookPath), allowFastInitialRefresh),
      config_(BibleConfigStore()) {}

void BibleActivity::onEnter() {
  // Ignore ReaderActivity::onEnter() call to keep recents intact
  // NOLINTNEXTLINE
  Activity::onEnter();

  sdFontSystem.ensureLoaded(renderer);
  applyInitialOrientation();

  const auto viewportWidth = renderer.getScreenWidth() - SETTINGS.screenMargin * 2;
  const auto viewportHeight = renderer.getScreenHeight() - SETTINGS.screenMargin * 2;

  renderSpec_ = SETTINGS.readerRenderSpec(viewportWidth, viewportHeight);

  if (!Storage.exists("/.bible")) {
    Storage.mkdir("/.bible");
  }
  if (!config_.loadFromFile()) {
    LOG_INF(MODULE_TAG, "Could not load configuration file");
  }

  if (bookPath.empty()) {
    bookPath = config_.biblePath;
    LOG_INF(MODULE_TAG, "No module path provided, using last opened: %s", bookPath.c_str());
  }

  if (loadBook()) {
    APP_STATE.openEpubPath = bookPath;
    auto _ = APP_STATE.saveToFile();

    chapterNavigator_.callback =
        [this](const BibleToolbox::BookPosition oldPosition, const BibleToolbox::BookPosition newPosition,
               const BibleToolbox::PositionChange changes, const BibleToolbox::NavDirection direction) {
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
          if (BibleToolbox::hasChange(changes,
                                      BibleToolbox::PositionChange::Book | BibleToolbox::PositionChange::Chapter)) {
            RECENT_BOOKS.updateBook(bookPath, getBookTitle(), chapterTitle_, "");
            this->loadChapter(direction);
          }
        };
  }
  requestUpdate();
}

void BibleActivity::onExit() { ReaderActivity::onExit(); }

void BibleActivity::loop() {
  ReaderActivity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    auto moduleId = bible_ == nullptr ? "None" : std::string(bible_->id());
    auto bookName = bible_ == nullptr ? "-" : std::string(chapterNavigator_.currentBookName());
    auto menu =
        std::make_unique<BibleMenuActivity>(renderer, mappedInput, moduleId, bookName, chapterNavigator_.getChapter());
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
  switch (menuItem) {
    case BibleMenuActivity::MODULE: {
      const std::filesystem::path modulePath{bookPath};
      const auto parent = modulePath.parent_path();
      auto browser =
          std::make_unique<FileBrowserActivity>(renderer, mappedInput, parent, FileBrowserActivity::Mode::Bibles);
      auto handler = [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& menuResult = std::get<FilePathResult>(result.data);
          LOG_DBG(MODULE_TAG, "Selected module path = %s", menuResult.path.c_str());
          bookPath = menuResult.path;
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
            loadChapter(BibleToolbox::NavFirstPage{});
          } else {
            LOG_DBG(MODULE_TAG, "Active book selected or out of range: %d", book);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
    }
    case BibleMenuActivity::CHAPTER:
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
      auto handler = [this, chapterCount](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          const auto chapter = menuResult.action + BibleToolbox::START_CHAPTER_NUMBER;
          if (chapterNavigator_.setPosition({.book = chapterNavigator_.getBook(), .chapter = chapter, .page = 0})) {
            loadChapter(BibleToolbox::NavFirstPage{});
          } else {
            LOG_DBG(MODULE_TAG, "Active chapter selected or out of range: %d", chapter);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
  }
}

void BibleActivity::renderPage(const int font_id, const int x, const int y) const {
  auto page = section_->loadPage(chapterNavigator_.getPage());
  if (page) {
    page->render(renderer, font_id, x, y);
  } else {
    LOG_ERR(MODULE_TAG, "Failed to load page from storage");
    section_->abandonBuild();
    auto _ = section_->clearCache();
  }
}

void BibleActivity::renderStatusBar() const {
  std::string title;
  if (SETTINGS.statusBarSpec().showsTitle()) {
    title = chapterTitle_;
  }
  GUI.drawStatusBar(renderer, chapterNavigator_.progress(), chapterNavigator_.getPage() + 1,
                    chapterNavigator_.totalPages, title);
}

bool BibleActivity::layout(const std::filesystem::path& cacheDir, BibleToolbox::NavDirection direction) {
  if (!section_) {
    auto cacheFile = cacheDir / "cache.html";
    auto binFile =
        cacheDir / std::format("{}_{}.bin", chapterNavigator_.currentBookNumber(), chapterNavigator_.getChapter());

    section_ = std::make_unique<BibleSection>(cacheFile, binFile, std::string(bible_->language()), renderer);

    const bool cacheLoaded = section_->loadSectionFile(renderSpec_);
    const bool cacheComplete = cacheLoaded && !section_->isPartial();

    const auto popup = [this]() {
      if (renderer.hasFrameBuffer()) {
        auto _ = GUI.drawPopup(renderer, tr(STR_INDEXING));
      }
    };

    const auto dumpParseFile = [this, &cacheFile]() {
      HalFile file = Storage.open(cacheFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
      if (!file) {
        LOG_ERR(MODULE_TAG, "Could not open cache file for writing %s", cacheFile.c_str());
        return false;
      }
      constexpr auto formatter = BibleVerseFormatter{};
      serialization::BufferedFileWriter cache{file, IO_BUFFER_SIZE};
      const serialization::BufferedFileWriterIterator iter{cache};
      formatter.formatChapter(iter, *bible_, chapterNavigator_.currentBookNumber(), chapterNavigator_.getChapter(),
                              bible_->chapterString());
      cache.flush();
      file.flush();
      file.close();
      LOG_DBG(MODULE_TAG, "Cache written to %s", cacheFile.c_str());
      return true;
    };

    const auto buildCache = [this, &popup, &dumpParseFile](const bool isPartial) {
      GfxRenderer::FrameBufferLoan loan(renderer);
      if (!dumpParseFile()) {
        return false;
      }
      if (!isPartial) {
        return section_->createSectionFile(renderSpec_, popup);
      }
      return section_->startBuild(renderSpec_, popup);
    };

    if (!cacheComplete) {
      if (section_->isPartial()) {
        LOG_DBG(MODULE_TAG, "Partial cache found (%d pages), resuming...", section_->pageCount);
      } else {
        LOG_DBG(MODULE_TAG, "Cache not found, building...");
      }
      if (!buildCache(section_->isPartial())) {
        LOG_ERR(MODULE_TAG, "Failed to create section cache file");
        section_.reset();
        return false;
      }
    } else {
      LOG_DBG(MODULE_TAG, "Cache found (%d pages)", section_->pageCount);
    }
  }

  if (section_->isBuilding()) {
    while (!section_->isBuildComplete()) {
      if (!section_->buildSomeMore(BUILD_PAGES_PER_CHUNK)) {
        section_.reset();
        return false;
      }
    }
  }

  chapterNavigator_.totalPages = static_cast<int>(section_->pageCount);
  if (section_->pageCount == 0) {
    return false;
  }
  std::visit(overloaded{[&](BibleToolbox::NavFirstPage) {
                          LOG_DBG(MODULE_TAG, "First page selected (was %d)", chapterNavigator_.getPage());
                          chapterNavigator_.setPage(0);
                        },
                        [&](BibleToolbox::NavLastPage) {
                          LOG_DBG(MODULE_TAG, "Last page selected: %d (was %d)", section_->pageCount,
                                  chapterNavigator_.getPage());
                          chapterNavigator_.setPage(chapterNavigator_.totalPages - 1);
                        },
                        [&](const BibleToolbox::NavTargetPage targetPage) {
                          LOG_DBG(MODULE_TAG, "Target page selected: %d (was %d)", targetPage,
                                  chapterNavigator_.getPage());
                          chapterNavigator_.setPage(targetPage.page);
                        }},
             direction);
  return true;
}

bool BibleActivity::isAtEndOfBook() const { return false; }

bool BibleActivity::loadChapter(const BibleToolbox::NavDirection direction) {
  section_.reset();
  chapterTitle_.clear();

  std::format_to(std::back_inserter(chapterTitle_), "{} {}", chapterNavigator_.currentBookName(),
                 chapterNavigator_.getChapter());

  // Update recents
  RECENT_BOOKS.addBook(bookPath, getBookTitle(), chapterTitle_, "");

  const std::filesystem::path cacheDir = std::filesystem::path("/") / ".bible" / std::string(bible_->id());

  if (!Storage.exists(cacheDir.c_str())) {
    Storage.mkdir(cacheDir.c_str());
  }

  return layout(cacheDir, direction);
}

bool BibleActivity::loadBook() {
  bible_ = std::make_unique<BibleToolbox::Bible>(bookPath, HAL_VFS_NAME);
  config_.biblePath = bookPath;  // Save loaded module

  if (bible_->books().empty()) {
    LOG_INF(MODULE_TAG, "Couldn't load Bible module (no books found)");
    config_.clear();  // Reset unloadable module
    return false;
  }

  bibleNavigator_.configureWith(bible_->books());
  if (!chapterNavigator_.setPosition(BibleToolbox::BookPosition{
          .book = config_.bookIndex, .chapter = config_.chapterNumber, .page = config_.pageNumber})) {
    LOG_INF(MODULE_TAG, "Position is out of bounds!");
  }

  return loadChapter(BibleToolbox::NavTargetPage{config_.pageNumber});
}

void BibleActivity::renderBook() {
  renderer.clearScreen();

  auto renderVerses = [&] {
    const int font_id = renderSpec_.fontId;
    const int x = SETTINGS.screenMargin;
    const int y = x;
    renderPage(font_id, x, y);
  };

  if (chapterNavigator_.totalPages > 0) {
    {
      auto* fontCacheManager = renderer.getFontCacheManager();
      auto scope = fontCacheManager->createPrewarmScope();
      renderVerses();
    }
    renderVerses();
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, 300, "No Bible module loaded", true, EpdFontFamily::BOLD);
  }

  renderStatusBar();

  if (SETTINGS.textAntiAliasing) {
    ReaderUtils::displayBaseWithRefreshCycle(renderer, pagesUntilFullRefresh);
    ReaderUtils::renderAntiAliased(renderer, renderVerses);
  } else {
    ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
  }
}

bool BibleActivity::pageTurn(const bool isForward) { return chapterNavigator_.turnPage(isForward); }

bool BibleActivity::skipPages(const int amount) { return chapterNavigator_.skipPages(amount); }

std::string BibleActivity::getBookTitle() const { return std::string(bible_->description()); }
