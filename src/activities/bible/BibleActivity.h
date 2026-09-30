#pragma once

#include <filesystem>

#include "Bible.h"
#include "BibleChapterSelectionActivity.h"
#include "BibleConfigStore.h"
#include "BibleMenuActivity.h"
#include "BibleNavigator.h"
#include "BibleSection.h"
#include "Epub/ReaderRenderSpec.h"
#include "PageNavigator.h"
#include "activities/Activity.h"
#include "activities/reader/ReaderActivity.h"

class BibleActivity final : public ReaderActivity {
  struct TargetPage {
    int page;
  };
  struct LastPage {};

  using PageNavigation = std::variant<TargetPage, LastPage>;

  std::unique_ptr<BibleSection> section_;
  ReaderRenderSpec renderSpec_{};
  std::unique_ptr<BibleToolbox::Bible> bible_;
  BibleToolbox::BibleNavigator bibleNavigator_{};
  BibleToolbox::PageNavigator<BibleToolbox::BibleNavigator> chapterNavigator_{bibleNavigator_};
  std::string chapterTitle_;
  std::vector<BibleChapterInfo> chapterListCache_;
  BibleConfigStore config_;

 protected:
  bool loadBook() override;
  std::string getBookTitle() const override;
  void renderBook() override;
  bool pageTurn(bool isForward) override;
  bool skipPages(int amount) override;
  bool isAtEndOfBook() const override;
  void onReturnFromEndOfBook() override {}
  void renderPage(int font_id, int x, int y) const;
  bool layout(const std::filesystem::path& cacheDir, PageNavigation pageNavigation);
  void renderStatusBar() const;
  bool loadChapter(PageNavigation pageNavigation);
  void handleMenuAction(BibleMenuActivity::MenuItem menuItem);
  void onPositionChanged(BibleToolbox::BookPosition oldPosition, BibleToolbox::BookPosition newPosition,
                         BibleToolbox::PositionChange changes);

 public:
  BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                bool allowFastInitialRefresh);
  ~BibleActivity() override = default;

  void loop() override;
  void onEnter() override;
  void onExit() override;
};
