#pragma once

#include <filesystem>

#include "BibleChapterSelectionActivity.h"
#include "BibleConfigStore.h"
#include "BibleMenuActivity.h"
#include "BibleSection.h"
#include "BibleToolbox.h"
#include "ChapterNavigator.h"
#include "Epub/ReaderRenderSpec.h"
#include "activities/Activity.h"
#include "activities/reader/ReaderActivity.h"

class BibleActivity final : public ReaderActivity {
  std::unique_ptr<BibleSection> section_;
  ReaderRenderSpec renderSpec_{};
  std::unique_ptr<BibleToolbox::Bible> bible_;
  BibleToolbox::ChapterNavigator chapterNavigator_{};
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
  void drawVerses(int font_id, int x, int y) const;
  bool layout(const std::filesystem::path& cachePath, BibleToolbox::ChapterNavigator::NavDirection direction);
  void renderStatusBar() const;
  bool loadChapter(bool clearCache, BibleToolbox::ChapterNavigator::NavDirection direction);
  void handleMenuAction(BibleMenuActivity::MenuItem menuItem);

 public:
  BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                bool allowFastInitialRefresh);
  ~BibleActivity() override = default;

  void loop() override;
  void onEnter() override;
  void onExit() override;
};
