#include "BaseBibleActivity.h"

#include <sys/stat.h>

#include <filesystem>
#include <ranges>

#include "../reader/ReaderUtils.h"
#include "BibleNavigator.h"
#include "BufferedFile.h"
#include "BufferedFileWriterIterator.h"
#include "CrossPointState.h"
#include "DailyNavigator.h"
#include "Epub/Page.h"
#include "Epub/parsers/ChapterHtmlSlimParser.h"
#include "FontCacheManager.h"
#include "I18n.h"
#include "I18nKeys.h"
#include "SdCardFontSystem.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr auto MODULE_TAG = "BIBLE";
constexpr size_t IO_BUFFER_SIZE = 4096;  // 4k in sync with BUILD_IO_BUFFER_SIZE
constexpr auto BUILD_PAGES_PER_CHUNK = 8;
}  // namespace

template <BibleToolbox::BookNavigable Navigator>
BaseBibleActivity<Navigator>::BaseBibleActivity(BibleToolbox::PageNavigator<Navigator> chapterNavigator,
                                                GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                std::string bookPath, const bool allowFastInitialRefresh)
    : ReaderActivity("BibleActivity", renderer, mappedInput, std::move(bookPath), allowFastInitialRefresh),
      chapterNavigator_(std::move(chapterNavigator)) {}

template <BibleToolbox::BookNavigable Navigator>
void BaseBibleActivity<Navigator>::onEnter() {
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

  if (loadBook()) {
    chapterNavigator_.callback =
        [this](const BibleToolbox::BookPosition oldPosition, const BibleToolbox::BookPosition newPosition,
               const BibleToolbox::PositionChange changes) { onPositionChanged(oldPosition, newPosition, changes); };
  }
  requestUpdate();
}

template <BibleToolbox::BookNavigable Navigator>
void BaseBibleActivity<Navigator>::renderPage(const int font_id, const int x, const int y) const {
  if (const auto page = section_->loadPage(chapterNavigator_.getPage())) {
    page->render(renderer, font_id, x, y);
  } else {
    LOG_ERR(MODULE_TAG, "Failed to load page from storage");
    section_->abandonBuild();
    auto _ = section_->clearCache();
  }
}

template <BibleToolbox::BookNavigable Navigator>
void BaseBibleActivity<Navigator>::renderStatusBar() const {
  std::string title;
  if (SETTINGS.statusBarSpec().showsTitle()) {
    title = getChapterTitle();
  }
  GUI.drawStatusBar(renderer, chapterNavigator_.progress(), chapterNavigator_.getPage() + 1,
                    chapterNavigator_.totalPages, title);
}

template <BibleToolbox::BookNavigable Navigator>
bool BaseBibleActivity<Navigator>::layout(const StartPagePosition startPagePosition) {
  if (!section_) {
    auto cacheFile = getCacheFileName();
    auto binFile = getBinFileName();

    section_ = std::make_unique<BibleSection>(cacheFile, binFile, getLanguage(), renderer);

    const bool cacheLoaded = section_->loadSectionFile(renderSpec_);
    const bool cacheComplete = cacheLoaded && !section_->isPartial();

    const auto popup = [this] {
      if (renderer.hasFrameBuffer()) {
        auto _ = GUI.drawPopup(renderer, tr(STR_INDEXING));
      }
    };

    const auto dumpParseFile = [this, &cacheFile] {
      HalFile file = Storage.open(cacheFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
      if (!file) {
        LOG_ERR(MODULE_TAG, "Could not open cache file for writing %s", cacheFile.c_str());
        return false;
      }
      serialization::BufferedFileWriter cache{file, IO_BUFFER_SIZE};
      const serialization::BufferedFileWriterIterator iter{cache};
      formatChapter(iter);
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

  if (const auto* target = std::get_if<TargetPage>(&startPagePosition)) {
    chapterNavigator_.setPage(target->page);
  } else if (std::holds_alternative<LastPage>(startPagePosition)) {
    chapterNavigator_.setPage(chapterNavigator_.totalPages - 1);
  }
  return true;
}

template <BibleToolbox::BookNavigable Navigator>
void BaseBibleActivity<Navigator>::renderBook() {
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

template <BibleToolbox::BookNavigable Navigator>
bool BaseBibleActivity<Navigator>::pageTurn(const bool isForward) {
  return chapterNavigator_.turnPage(isForward);
}

template <BibleToolbox::BookNavigable Navigator>
bool BaseBibleActivity<Navigator>::skipPages(const int amount) {
  return chapterNavigator_.skipPages(amount);
}

template class BaseBibleActivity<BibleToolbox::DailyNavigator>;
template class BaseBibleActivity<BibleToolbox::BibleNavigator>;
