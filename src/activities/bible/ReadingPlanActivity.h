#pragma once

#include "BaseBibleActivity.h"
#include "BibleConfigStore.h"
#include "DailyNavigator.h"
#include "ReadingPlan.h"

class ReadingPlanActivity final : public BaseBibleActivity<BibleToolbox::DailyNavigator> {
  BibleToolbox::DailyNavigator dailyNavigator_{};

 protected:
  std::shared_ptr<BibleToolbox::Bible> bible_;
  std::unique_ptr<BibleToolbox::ReadingPlan> readingPlan_;
  BibleConfigStore& config_;

  std::filesystem::path getCacheDir() const override;
  std::string getCacheFileName() const override;
  std::string getBinFileName() const override;
  std::string getChapterTitle() const override;
  std::string getBookTitle() const override;
  std::string getLanguage() const override;
  bool isAtEndOfBook() const override;
  bool loadBook() override;
  bool loadChapter(StartPagePosition startPagePosition) override;
  void formatChapter(serialization::BufferedFileWriterIterator iter) override;
  void onPositionChanged(BibleToolbox::BookPosition oldPosition, BibleToolbox::BookPosition newPosition,
                         BibleToolbox::PositionChange changes) override;

 public:
  ReadingPlanActivity(const std::shared_ptr<BibleToolbox::Bible>& bible, BibleConfigStore& config,
                      GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                      const bool allowFastInitialRefresh)
      : BaseBibleActivity(BibleToolbox::PageNavigator{dailyNavigator_}, renderer, mappedInput, std::move(bookPath),
                          allowFastInitialRefresh),
        bible_(bible),
        config_(config) {}

  void loop() override;
};
