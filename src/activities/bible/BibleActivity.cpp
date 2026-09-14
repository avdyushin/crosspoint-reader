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
#include "SdCardFontSystem.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BibleVerseFormatter.h"

namespace {

constexpr auto MODULE_TAG = "BIBLE";
constexpr size_t MIN_STYLED_FREE_HEAP = 40 * 1024;
constexpr size_t MIN_STYLED_MAX_ALLOC = 20 * 1024;
constexpr size_t MAX_STYLED_PAGE_ELEMENTS = 1024;
constexpr size_t MAX_STYLED_PAGES = 256;
constexpr size_t IO_BUFFER_SIZE = 4096;  // 4k in sync with BUILD_IO_BUFFER_SIZE

bool buildPages(GfxRenderer& renderer, const std::filesystem::path& path, const BibleActivity::Config& config,
                std::vector<std::unique_ptr<Page>>& pages) {
  if (ESP.getFreeHeap() < MIN_STYLED_FREE_HEAP || ESP.getMaxAllocHeap() < MIN_STYLED_MAX_ALLOC) {
    LOG_ERR(MODULE_TAG, "Low heap for styled chapter (%u free, %u max block)", ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    return false;
  }
  pages.clear();
  pages.reserve(MAX_STYLED_PAGES);
  bool parsed = false;
  bool resourceLimitHit = false;
  {
    size_t retainedElements = 0;
    const char* limitReason = nullptr;
    constexpr int image_rendering = 2;
    constexpr auto image_base = "";
    constexpr auto content_base = "";
    constexpr bool embedded_style = false;
    auto page_func = [&pages, &resourceLimitHit, &retainedElements, &limitReason](std::unique_ptr<Page> page, uint16_t,
                                                                                  uint16_t, uint32_t) {
      if (resourceLimitHit) {
        return;
      }
      const size_t page_elements = page->elements.size();
      if (pages.size() > MAX_STYLED_PAGES) {
        limitReason = "Too many pages";
      } else if (page_elements > MAX_STYLED_PAGE_ELEMENTS - retainedElements) {
        limitReason = "Too many page elements";
      } else if (ESP.getFreeHeap() < MIN_STYLED_FREE_HEAP || ESP.getMaxAllocHeap() < MIN_STYLED_MAX_ALLOC) {
        limitReason = "No free heap left";
      }
      if (limitReason != nullptr) {
        LOG_ERR(MODULE_TAG, "Parser failed %s", limitReason);
        resourceLimitHit = true;
        pages.clear();
        return;
      }
      retainedElements += page_elements;
      pages.push_back(std::move(page));
    };
    const auto parser = makeUniqueNoThrow<ChapterHtmlSlimParser>(
        nullptr, path.string(), renderer, config.fontId, config.lineCompression, config.extraParagraphSpacing,
        config.paragraphAlignment, config.viewportWidth, config.viewportHeight, config.hyphenationEnabled,
        config.focusReadingEnabled, page_func, embedded_style, content_base, image_base, image_rendering);

    if (!parser) {
      LOG_ERR(MODULE_TAG, "Out of memory!");
    } else {
      parsed = parser->parseAndBuildPages();
    }
  }
  if (resourceLimitHit) {
    LOG_ERR(MODULE_TAG, "Renderer exceeded page heap limit");
    return false;
  }
  if (!parsed || pages.empty()) {
    LOG_ERR(MODULE_TAG, "Can't parse or pages are empty");
    return false;
  }
  return true;
}

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

}  // namespace

BibleActivity::BibleActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput)
    : ReaderActivity(name, renderer, mappedInput, "", false) {}

void BibleActivity::onEnter() {
  // Ignore ReaderActivity::onEnter() call to keep recents intact
  // NOLINTNEXTLINE
  Activity::onEnter();

  Storage.mkdir(".bible");

  sdFontSystem.ensureLoaded(renderer);
  applyInitialOrientation();

  LOG_INF(MODULE_TAG, "Opened");

  config_ = Config{
      .fontId = SETTINGS.getReaderFontId(),
      .lineCompression = SETTINGS.getReaderLineCompression(),
      .extraParagraphSpacing = SETTINGS.extraParagraphSpacing,
      .marginLeft = SETTINGS.screenMargin,
      .marginTop = SETTINGS.screenMargin,
      .marginRight = SETTINGS.screenMargin,
      .marginBottom = SETTINGS.screenMargin,
      .viewportWidth = renderer.getScreenWidth() - SETTINGS.screenMargin * 2,
      .viewportHeight = renderer.getScreenHeight() - SETTINGS.screenMargin * 2,
  };

  loadBook();
  requestUpdate();
}

void BibleActivity::onExit() {
  ReaderActivity::onExit();

  BibleConfigStore::getInstance().config.bookIndex = chapterNavigator_.currentBookIndex;
  BibleConfigStore::getInstance().config.chapterNumber = chapterNavigator_.inBookChapter;
  BibleConfigStore::getInstance().config.pageNumber = chapterNavigator_.currentPage;
  auto _ = BibleConfigStore::getInstance().saveToFile();
  BibleContext::activeBible.store(nullptr, std::memory_order_release);
}

void BibleActivity::loop() {
  ReaderActivity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    auto menu = std::make_unique<BibleMenuActivity>(
        renderer, mappedInput, bible_->id(), chapterNavigator_.currentBook()->name, chapterNavigator_.inBookChapter);
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
    case BibleMenuActivity::MODULE:
      LOG_INF(MODULE_TAG, "Module activated");
      break;
    case BibleMenuActivity::BOOK: {
      LOG_INF(MODULE_TAG, "Book activated");
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
            loadChapter(true, BibleChapterNavigator::NavFirstPage{});
          } else {
            LOG_INF(MODULE_TAG, "Same book selected or out of range: %d", targetBookIndex);
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
      LOG_INF(MODULE_TAG, "Chapter activated, total chapters: %d", totalChapters);
      auto chapterView = std::views::iota(BibleChapterNavigator::START_CHAPTER_NUMBER, totalChapters + 1) |
                         std::views::transform([chapterString](const int i) {
                           return BibleChapterInfo{.name = std::string(chapterString) + " " + std::to_string(i)};
                         });
      std::ranges::copy(chapterView, std::back_inserter(chapterListCache_));
      auto menu = std::make_unique<BibleChapterSelectionActivity>(
          renderer, mappedInput, "Select Chapter", chapterListCache_, chapterNavigator_.currentBookIndex);
      auto handler = [this, totalChapters](const ActivityResult& result) {
        const auto& menuResult = std::get<MenuResult>(result.data);
        if (!result.isCancelled) {
          LOG_INF(MODULE_TAG, "Selected Chapter number = %d", menuResult.action);
          if (const auto targetChapterNumber = menuResult.action + BibleChapterNavigator::START_CHAPTER_NUMBER;
              targetChapterNumber >= BibleChapterNavigator::START_CHAPTER_NUMBER &&
              targetChapterNumber <= totalChapters && targetChapterNumber != chapterNavigator_.inBookChapter) {
            chapterNavigator_.inBookChapter = targetChapterNumber;
            chapterNavigator_.currentPage = 0;
            loadChapter(true, BibleChapterNavigator::NavFirstPage{});
          } else {
            LOG_INF(MODULE_TAG, "Same chapter selected or out of range: %d", targetChapterNumber);
          }
        }
        requestUpdate();
      };
      startActivityForResult(std::move(menu), handler);
      break;
  }
}

void BibleActivity::drawVerses(const int font_id, const int x, const int y) const {
  if (!pages_.empty()) {
    pages_[chapterNavigator_.currentPage]->render(renderer, font_id, x, y);
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
  buildPages(renderer, cachePath, config_, pages_);
  chapterNavigator_.totalPages = static_cast<int>(pages_.size());
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
  title_.clear();
  std::format_to(std::back_inserter(title_), "{} {}", chapterNavigator_.currentBook()->name,
                 chapterNavigator_.inBookChapter);

  const std::filesystem::path cache_path = std::filesystem::path(".bible") / std::string(bible_->id()) / "cache.html";

  {
    if (clearCache || !Storage.exists(cache_path.c_str())) {
      HalFile file = Storage.open(cache_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
      if (!file) {
        LOG_ERR(MODULE_TAG, "Could not open cache file for writing %s", cache_path.c_str());
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
  layout(cache_path, direction);
  return true;
}

bool BibleActivity::loadBook() {
  if (!BibleConfigStore::getInstance().loadFromFile()) {
    LOG_INF(MODULE_TAG, "Could not load configuration file");
  }

  const auto config = BibleConfigStore::getInstance().config;

  const std::filesystem::path database_path = std::filesystem::path("bible") / "modules";

  std::string filename;
  if (config.module.empty()) {
    if (auto const files = Storage.listFiles(database_path.c_str()); !files.empty()) {
      filename = files.front();
    }
  } else {
    filename = config.module + ".SQLite3";
  }

  try {
    bible_ = std::make_unique<BibleToolbox::Bible>(database_path / filename);
    BibleContext::activeBible.store(bible_.get(), std::memory_order_release);
    BibleConfigStore::getInstance().config.module = bible_->id();  // Save loaded module
  } catch (const std::exception& e) {
    LOG_INF(MODULE_TAG, "Could create bible instance: %s", e.what());
    BibleContext::activeBible.store(nullptr, std::memory_order_release);
    BibleConfigStore::getInstance().config.clear();  // Reset unloadable module
    return false;
  }

  chapterNavigator_.books = bible_->books();
  chapterNavigator_.currentBookIndex = config.bookIndex;
  chapterNavigator_.inBookChapter = config.chapterNumber;
  chapterNavigator_.onChapterChanged = [this](const int bookIndex, const int chapterIndex,
                                              const BibleChapterNavigator::NavDirection direction) {
    LOG_INF(MODULE_TAG, "Book index %d chapter %d direction %d", bookIndex, chapterIndex, direction);
    this->loadChapter(true, direction);
  };

  // One current chapter saves, clear cache can be false
  return loadChapter(true, BibleChapterNavigator::NavTargetPage{config.pageNumber});
}

bool BibleActivity::pageTurn(const bool isForward) {
  return isForward ? chapterNavigator_.nextPageOrChapter() : chapterNavigator_.previousPageOrChapter();
}

void BibleActivity::renderBook() {
  renderer.clearScreen();

  auto renderVerses = [&] {
    const int font_id = config_.fontId;
    const int x = config_.marginLeft;
    const int y = config_.marginTop;
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
