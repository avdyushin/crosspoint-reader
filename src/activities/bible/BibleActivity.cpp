#include "BibleActivity.h"

#include <BibleToolbox.h>
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

  chapterNavigator_.onPageChanged = [this](const int page) { config_.pageNumber = page; };

  if (loadBook()) {
    APP_STATE.openEpubPath = bookPath;
    auto _ = APP_STATE.saveToFile();

    chapterNavigator_.onChapterChanged = [this](const int bookIndex, const int chapterIndex,
                                                const BibleToolbox::ChapterNavigator::NavDirection direction) {
      LOG_INF(MODULE_TAG, "Book index %d chapter %d direction %d", bookIndex, chapterIndex, direction);
      config_.bookIndex = chapterNavigator_.currentBookIndex;
      config_.chapterNumber = chapterNavigator_.inBookChapter;
      RECENT_BOOKS.updateBook(bookPath, getBookTitle(), chapterTitle_, "");
      this->loadChapter(true, direction);
    };
  }
  requestUpdate();
}

void BibleActivity::onExit() { ReaderActivity::onExit(); }

void BibleActivity::loop() {
  ReaderActivity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    auto moduleId = bible_ == nullptr ? "None" : std::string(bible_->id());
    auto bookName = bible_ == nullptr ? "-" : chapterNavigator_.currentBook()->name;
    auto menu =
        std::make_unique<BibleMenuActivity>(renderer, mappedInput, moduleId, bookName, chapterNavigator_.inBookChapter);
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
                                                               chapterNavigator_.currentBookIndex);
      auto handler = [this](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          LOG_INF(MODULE_TAG, "Selected Book index = %d", menuResult.action);
          if (const auto targetBookIndex = menuResult.action; targetBookIndex >= 0 &&
                                                              targetBookIndex < chapterNavigator_.books.size() &&
                                                              targetBookIndex != chapterNavigator_.currentBookIndex) {
            chapterNavigator_.currentBookIndex = targetBookIndex;
            chapterNavigator_.inBookChapter = BibleToolbox::START_CHAPTER_NUMBER;
            chapterNavigator_.setCurrentPage(0);
            config_.bookIndex = targetBookIndex;
            config_.chapterNumber = BibleToolbox::START_CHAPTER_NUMBER;
            loadChapter(true, BibleToolbox::ChapterNavigator::NavFirstPage{});
          } else {
            LOG_DBG(MODULE_TAG, "Active book selected or out of range: %d", targetBookIndex);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
    }
    case BibleMenuActivity::CHAPTER:
      const auto totalChapters = chapterNavigator_.currentBook()->chaptersCount;
      const auto chapterString = bible_->chapterString();
      chapterListCache_.clear();
      chapterListCache_.reserve(totalChapters);
      auto chapterView = std::views::iota(BibleToolbox::START_CHAPTER_NUMBER, totalChapters + 1) |
                         std::views::transform([chapterString](const int i) {
                           return BibleChapterInfo{.name = std::string(chapterString) + " " + std::to_string(i)};
                         });
      std::ranges::copy(chapterView, std::back_inserter(chapterListCache_));
      auto menu = std::make_unique<BibleChapterSelectionActivity>(
          renderer, mappedInput, "Select Chapter", chapterListCache_,
          chapterNavigator_.inBookChapter - BibleToolbox::START_CHAPTER_NUMBER);
      auto handler = [this, totalChapters](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          if (const auto targetChapterNumber = menuResult.action + BibleToolbox::START_CHAPTER_NUMBER;
              targetChapterNumber >= BibleToolbox::START_CHAPTER_NUMBER && targetChapterNumber <= totalChapters &&
              targetChapterNumber != chapterNavigator_.inBookChapter) {
            chapterNavigator_.inBookChapter = targetChapterNumber;
            chapterNavigator_.setCurrentPage(0);
            config_.chapterNumber = targetChapterNumber;
            loadChapter(true, BibleToolbox::ChapterNavigator::NavFirstPage{});
          } else {
            LOG_DBG(MODULE_TAG, "Active chapter selected or out of range: %d", targetChapterNumber);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
  }
}

void BibleActivity::drawVerses(const int font_id, const int x, const int y) const {
  auto page = section_->loadPage(chapterNavigator_.getCurrentPage());
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
  GUI.drawStatusBar(renderer, chapterNavigator_.progress(), chapterNavigator_.getCurrentPage() + 1,
                    chapterNavigator_.totalPages, title);
}

bool BibleActivity::layout(const std::filesystem::path& cachePath,
                           BibleToolbox::ChapterNavigator::NavDirection direction) {
  if (!section_) {
    section_ = std::make_unique<BibleSection>(cachePath.parent_path(), std::string(bible_->language()),
                                              chapterNavigator_.currentBookNumber(), chapterNavigator_.inBookChapter,
                                              renderer);

    const bool cacheLoaded = section_->loadSectionFile(renderSpec_);
    const bool cacheComplete = cacheLoaded && !section_->isPartial();

    if (!cacheComplete) {
      if (section_->isPartial()) {
        LOG_DBG(MODULE_TAG, "Partial cache found (%d pages), resuming...", section_->pageCount);
      } else {
        LOG_DBG(MODULE_TAG, "Cache not found, building...");
      }
    } else {
      LOG_DBG(MODULE_TAG, "Cache found (%d pages)", section_->pageCount);
    }

    auto _ = GUI.drawPopup(renderer, tr(STR_INDEXING));
    const auto popup = [this]() {
      if (renderer.hasFrameBuffer()) {
        auto _ = GUI.drawPopup(renderer, tr(STR_INDEXING));
      }
    };

    GfxRenderer::FrameBufferLoan loan(renderer);
    if (!section_->createSectionFile(renderSpec_, popup)) {
      LOG_ERR(MODULE_TAG, "Failed to create section file");
      section_.reset();
      loan.end();
      return false;
    }
    loan.end();
  }

  chapterNavigator_.totalPages = static_cast<int>(section_->pageCount);
  if (section_->pageCount == 0) {
    return false;
  }
  std::visit(overloaded{[&](BibleToolbox::ChapterNavigator::NavFirstPage) {
                          LOG_DBG(MODULE_TAG, "First page selected");
                          chapterNavigator_.setCurrentPage(0);
                        },
                        [&](BibleToolbox::ChapterNavigator::NavLastPage) {
                          LOG_DBG(MODULE_TAG, "Last page selected: %d", section_->pageCount);
                          chapterNavigator_.setCurrentPage(chapterNavigator_.totalPages - 1);
                        },
                        [&](const BibleToolbox::ChapterNavigator::NavTargetPage targetPage) {
                          LOG_DBG(MODULE_TAG, "Target page %d", targetPage);
                          chapterNavigator_.setCurrentPage(targetPage.page);
                        }},
             direction);
  return true;
}

bool BibleActivity::isAtEndOfBook() const { return false; }

bool BibleActivity::loadChapter(const bool clearCache, const BibleToolbox::ChapterNavigator::NavDirection direction) {
  section_.reset();
  chapterTitle_.clear();

  std::format_to(std::back_inserter(chapterTitle_), "{} {}", chapterNavigator_.currentBook()->name,
                 chapterNavigator_.inBookChapter);

  // Update recents
  RECENT_BOOKS.addBook(bookPath, getBookTitle(), chapterTitle_, "");

  const std::filesystem::path cacheDir = std::filesystem::path("/") / ".bible" / std::string(bible_->id());

  if (!Storage.exists(cacheDir.c_str())) {
    Storage.mkdir(cacheDir.c_str());
  }

  const std::filesystem::path cachePath = cacheDir / "cache.html";

  {
    if (clearCache || !Storage.exists(cachePath.c_str())) {
      HalFile file = Storage.open(cachePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
      if (!file) {
        LOG_ERR(MODULE_TAG, "Could not open cache file for writing %s", cachePath.c_str());
        return false;
      }
      constexpr auto formatter = BibleVerseFormatter{};
      serialization::BufferedFileWriter cache{file, IO_BUFFER_SIZE};
      const serialization::BufferedFileWriterIterator iter{cache};
      formatter.formatChapter(iter, *bible_, chapterNavigator_.currentBookNumber(), chapterNavigator_.inBookChapter,
                              bible_->chapterString());
      // will flush and file via destructors
    } else {
      LOG_DBG(MODULE_TAG, "Loading cache file");
    }
  }
  return layout(cachePath, direction);
}

bool BibleActivity::loadBook() {
  bible_ = std::make_unique<BibleToolbox::Bible>(bookPath, HAL_VFS_NAME);
  config_.biblePath = bookPath;  // Save loaded module

  if (bible_->books().empty()) {
    LOG_INF(MODULE_TAG, "Couldn't load Bible module (no books found)");
    config_.clear();  // Reset unloadable module
    return false;
  }

  chapterNavigator_.books = bible_->books();
  chapterNavigator_.currentBookIndex = config_.bookIndex;
  chapterNavigator_.inBookChapter = config_.chapterNumber;

  return loadChapter(true, BibleToolbox::ChapterNavigator::NavTargetPage{config_.pageNumber});
}

void BibleActivity::renderBook() {
  renderer.clearScreen();

  auto renderVerses = [&] {
    const int font_id = renderSpec_.fontId;
    const int x = SETTINGS.screenMargin;
    const int y = x;
    drawVerses(font_id, x, y);
  };

  if (chapterNavigator_.totalPages > 0) {
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
