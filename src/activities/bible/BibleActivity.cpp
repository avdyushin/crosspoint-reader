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
#include "Epub/Page.h"
#include "Epub/parsers/ChapterHtmlSlimParser.h"
#include "FontCacheManager.h"
#include "I18n.h"
#include "I18nKeys.h"
#include "SdCardFontSystem.h"
#include "activities/home/FileBrowserActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BibleVerseFormatter.h"

namespace {

constexpr auto MODULE_TAG = "BIBLE";
constexpr size_t IO_BUFFER_SIZE = 4096;  // 4k in sync with BUILD_IO_BUFFER_SIZE

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

}  // namespace

BibleActivity::BibleActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput)
    : ReaderActivity(name, renderer, mappedInput, "", false),
      databasePath_(std::filesystem::path("/") / "bible" / "modules") {}

void BibleActivity::onEnter() {
  // Ignore ReaderActivity::onEnter() call to keep recents intact
  // NOLINTNEXTLINE
  Activity::onEnter();

  if (!Storage.exists("/.bible")) {
    Storage.mkdir("/.bible");
  }
  if (!Storage.exists(databasePath_.c_str())) {
    Storage.mkdir(databasePath_.c_str());
  }

  sdFontSystem.ensureLoaded(renderer);
  applyInitialOrientation();

  LOG_INF(MODULE_TAG, "Opened");

  const auto viewportWidth = renderer.getScreenWidth() - SETTINGS.screenMargin * 2;
  const auto viewportHeight = renderer.getScreenHeight() - SETTINGS.screenMargin * 2;

  renderSpec_ = SETTINGS.readerRenderSpec(viewportWidth, viewportHeight);

  if (!BibleConfigStore::getInstance().loadFromFile()) {
    LOG_INF(MODULE_TAG, "Could not load configuration file");
  }

  loadBook();
  requestUpdate();
}

void BibleActivity::onExit() {
  ReaderActivity::onExit();

  BibleConfigStore::getInstance().config.bookIndex = chapterNavigator_.currentBookIndex;
  BibleConfigStore::getInstance().config.chapterNumber = chapterNavigator_.inBookChapter;
  BibleConfigStore::getInstance().config.pageNumber = chapterNavigator_.currentPage;
  auto _ = BibleConfigStore::getInstance().saveToFile();
}

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
      auto browser = std::make_unique<FileBrowserActivity>(renderer, mappedInput, databasePath_.string(),
                                                           FileBrowserActivity::Mode::Bibles);
      auto handler = [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& menuResult = std::get<FilePathResult>(result.data);
          LOG_DBG(MODULE_TAG, "Selected module path = %s", menuResult.path.c_str());
          BibleConfigStore::getInstance().config.module = menuResult.path;  // Save loaded module
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
            chapterNavigator_.inBookChapter = BibleChapterNavigator::START_CHAPTER_NUMBER;
            chapterNavigator_.currentPage = 0;
            BibleConfigStore::getInstance().config.bookIndex = targetBookIndex;
            BibleConfigStore::getInstance().config.chapterNumber = BibleChapterNavigator::START_CHAPTER_NUMBER;
            BibleConfigStore::getInstance().config.pageNumber = 0;
            loadChapter(true, BibleChapterNavigator::NavFirstPage{});
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
      auto chapterView = std::views::iota(BibleChapterNavigator::START_CHAPTER_NUMBER, totalChapters + 1) |
                         std::views::transform([chapterString](const int i) {
                           return BibleChapterInfo{.name = std::string(chapterString) + " " + std::to_string(i)};
                         });
      std::ranges::copy(chapterView, std::back_inserter(chapterListCache_));
      auto menu = std::make_unique<BibleChapterSelectionActivity>(
          renderer, mappedInput, "Select Chapter", chapterListCache_,
          chapterNavigator_.inBookChapter - BibleChapterNavigator::START_CHAPTER_NUMBER);
      auto handler = [this, totalChapters](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          if (const auto targetChapterNumber = menuResult.action + BibleChapterNavigator::START_CHAPTER_NUMBER;
              targetChapterNumber >= BibleChapterNavigator::START_CHAPTER_NUMBER &&
              targetChapterNumber <= totalChapters && targetChapterNumber != chapterNavigator_.inBookChapter) {
            chapterNavigator_.inBookChapter = targetChapterNumber;
            chapterNavigator_.currentPage = 0;
            BibleConfigStore::getInstance().config.chapterNumber = targetChapterNumber;
            BibleConfigStore::getInstance().config.pageNumber = 0;
            loadChapter(true, BibleChapterNavigator::NavFirstPage{});
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
  auto page = section_->loadPage(chapterNavigator_.currentPage);
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
    title = title_;
  }
  GUI.drawStatusBar(renderer, chapterNavigator_.progress(), chapterNavigator_.currentPage + 1,
                    chapterNavigator_.totalPages, title);
}

bool BibleActivity::layout(const std::filesystem::path& cachePath, BibleChapterNavigator::NavDirection direction) {
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
  std::visit(
      overloaded{
          [&](BibleChapterNavigator::NavFirstPage) { chapterNavigator_.currentPage = 0; },
          [&](BibleChapterNavigator::NavLastPage) { chapterNavigator_.currentPage = chapterNavigator_.totalPages - 1; },
          [&](const BibleChapterNavigator::NavTargetPage targetPage) {
            chapterNavigator_.currentPage = std::clamp(targetPage.page, 0, chapterNavigator_.totalPages - 1);
          }},
      direction);
  return true;
}

bool BibleActivity::isAtEndOfBook() const { return false; }

bool BibleActivity::loadChapter(const bool clearCache, const BibleChapterNavigator::NavDirection direction) {
  section_.reset();
  title_.clear();

  std::format_to(std::back_inserter(title_), "{} {}", chapterNavigator_.currentBook()->name,
                 chapterNavigator_.inBookChapter);

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
  const auto config = BibleConfigStore::getInstance().config;

  std::string filename;
  if (config.module.empty()) {
    if (auto const files = Storage.listFiles(databasePath_.c_str()); !files.empty()) {
      filename = files[0].c_str();
    }
  } else {
    filename = config.module;
  }

  if (filename.empty()) {
    LOG_INF(MODULE_TAG, "No Bible module find");
    BibleConfigStore::getInstance().config.clear();  // Reset unloadable module
    return false;
  }

  const auto modulePath = databasePath_ / filename;
  bible_ = std::make_unique<BibleToolbox::Bible>(modulePath);
  BibleConfigStore::getInstance().config.module = modulePath;  // Save loaded module

  if (bible_->books().empty()) {
    LOG_INF(MODULE_TAG, "Could create bible instance");
    BibleConfigStore::getInstance().config.clear();  // Reset unloadable module
    return false;
  }

  LOG_INF(MODULE_TAG, "Loading config data, book %d, chapter %d and page %d", config.bookIndex, config.chapterNumber,
          config.pageNumber);
  chapterNavigator_.books = bible_->books();
  chapterNavigator_.currentBookIndex = config.bookIndex;
  chapterNavigator_.inBookChapter = config.chapterNumber;
  chapterNavigator_.onChapterChanged = [this](const int bookIndex, const int chapterIndex,
                                              const BibleChapterNavigator::NavDirection direction) {
    LOG_INF(MODULE_TAG, "Book index %d chapter %d direction %d", bookIndex, chapterIndex, direction);
    BibleConfigStore::getInstance().config.bookIndex = chapterNavigator_.currentBookIndex;
    BibleConfigStore::getInstance().config.chapterNumber = chapterNavigator_.inBookChapter;
    this->loadChapter(true, direction);
  };

  return loadChapter(true, BibleChapterNavigator::NavTargetPage{config.pageNumber});
}

bool BibleActivity::pageTurn(const bool isForward) {
  const auto result = isForward ? chapterNavigator_.nextPageOrChapter() : chapterNavigator_.previousPageOrChapter();
  if (result) {
    BibleConfigStore::getInstance().config.pageNumber = chapterNavigator_.currentPage;
  }
  return result;
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

bool BibleActivity::skipPages(const int amount) { return chapterNavigator_.skipPages(amount); }
