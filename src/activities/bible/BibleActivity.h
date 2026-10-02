#pragma once

#include "BaseBibleActivity.h"
#include "BibleChapterSelectionActivity.h"
#include "BibleConfigStore.h"
#include "BibleMenuActivity.h"
#include "BibleNavigator.h"
#include "ReadingPlanConfigStore.h"

class BibleActivity final : public BaseBibleActivity<BibleToolbox::BibleNavigator> {
  BibleConfigStore config_{};
  ReadingPlanConfigStore readingPlanConfig_{};
  BibleToolbox::BibleNavigator bibleNavigator_{};
  std::vector<BibleChapterInfo> chapterListCache_;

  void handleMenuAction(BibleMenuActivity::MenuItem menuItem);

 protected:
  std::shared_ptr<BibleToolbox::Bible> bible_;

  std::filesystem::path getCacheDir() const override;
  std::string getCacheFileName() const override;
  std::string getBinFileName() const override;
  std::string getChapterTitle() const override;
  std::string getBookTitle() const override;
  std::string getLanguage() const override;
  bool isAtEndOfBook() const override { return false; }
  bool loadBook() override;
  bool loadChapter(StartPagePosition startPagePosition) override;
  void formatChapter(serialization::BufferedFileWriterIterator iter) override;
  void onPositionChanged(BibleToolbox::BookPosition oldPosition, BibleToolbox::BookPosition newPosition,
                         BibleToolbox::PositionChange changes) override;

 public:
  BibleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                const bool allowFastInitialRefresh)
      : BaseBibleActivity("BibleActivity", BibleToolbox::PageNavigator{bibleNavigator_}, renderer, mappedInput,
                          std::move(bookPath), allowFastInitialRefresh) {}

  void loop() override;
};
