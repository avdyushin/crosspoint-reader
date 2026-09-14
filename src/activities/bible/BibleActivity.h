#pragma once

#include <filesystem>

#include "BibleChapterNavigator.h"
#include "BibleChapterSelectionActivity.h"
#include "BibleMenuActivity.h"
#include "BibleToolbox.h"
#include "Epub/Page.h"
#include "activities/Activity.h"
#include "activities/reader/ReaderActivity.h"

class BibleActivity final : public ReaderActivity {
 public:
  struct Config {
    int fontId;
    float lineCompression;
    uint16_t paragraphAlignment;
    uint16_t extraParagraphSpacing;
    uint8_t marginLeft;
    uint8_t marginTop;
    uint8_t marginRight;
    uint8_t marginBottom;
    int viewportWidth;
    int viewportHeight;
    uint8_t hyphenationEnabled;
    uint8_t focusReadingEnabled;
  };

 private:
  struct SaveState {
    std::string module;
    int bookIndex;
    int chapterNumber;
    int pageNumber;
  };

  std::vector<std::unique_ptr<Page>> pages_;
  Config config_{};
  std::unique_ptr<BibleToolbox::Bible> bible_;
  BibleChapterNavigator chapterNavigator_{};
  std::string title_;
  std::vector<BibleChapterInfo> chapterListCache_;
  const std::filesystem::path databasePath;

 protected:
  bool loadBook() override;
  std::string getBookTitle() const override { return ""; }
  void renderBook() override;
  bool pageTurn(bool isForward) override;
  bool skipPages(int amount) override;
  bool isAtEndOfBook() const override;
  void onReturnFromEndOfBook() override {}
  void drawVerses(int font_id, int x, int y) const;
  bool layout(const std::filesystem::path& cachePath, BibleChapterNavigator::NavDirection direction);
  void renderStatusBar() const;
  bool loadChapter(bool clearCache, BibleChapterNavigator::NavDirection direction);
  void handleMenuAction(BibleMenuActivity::MenuItem menuItem);

 public:
  BibleActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~BibleActivity() override = default;

  void loop() override;
  void onEnter() override;
  void onExit() override;
};
