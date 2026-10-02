#pragma once

#include <filesystem>

#include "BibleSection.h"
#include "BufferedFileWriterIterator.h"
#include "Epub/ReaderRenderSpec.h"
#include "PageNavigator.h"
#include "activities/Activity.h"
#include "activities/reader/ReaderActivity.h"

template <BibleToolbox::BookNavigable Navigator>
class BaseBibleActivity : public ReaderActivity {
  ReaderRenderSpec renderSpec_{};

 protected:
  struct TargetPage {
    int page;
  };
  struct LastPage {};

  using StartPagePosition = std::variant<TargetPage, LastPage>;

  std::unique_ptr<BibleSection> section_;
  BibleToolbox::PageNavigator<Navigator> chapterNavigator_;

  void renderBook() override;
  bool pageTurn(bool isForward) override;
  bool skipPages(int amount) override;
  void onReturnFromEndOfBook() override {}

  void renderPage(int font_id, int x, int y) const;
  bool layout(StartPagePosition startPagePosition);
  void renderStatusBar() const;

  virtual std::string getChapterTitle() const = 0;
  virtual std::string getLanguage() const = 0;
  virtual void formatChapter(serialization::BufferedFileWriterIterator iter) = 0;
  virtual bool loadChapter(StartPagePosition startPagePosition) = 0;
  virtual void onPositionChanged(BibleToolbox::BookPosition oldPosition, BibleToolbox::BookPosition newPosition,
                                 BibleToolbox::PositionChange changes) = 0;
  virtual std::filesystem::path getCacheDir() const = 0;
  virtual std::string getCacheFileName() const = 0;
  virtual std::string getBinFileName() const = 0;

 public:
  BaseBibleActivity(const char* name, BibleToolbox::PageNavigator<Navigator>, GfxRenderer& renderer,
                    MappedInputManager& mappedInput, std::string bookPath, bool allowFastInitialRefresh);
  ~BaseBibleActivity() override = default;

  void onEnter() override;
};
