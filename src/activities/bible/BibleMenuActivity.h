#pragma once
#include <utility>

#include "activities/UiListActivity.h"

class BibleMenuActivity final : public UiListActivity {
 public:
  struct Config {
    std::string currentModuleId;
    std::string currentBookName;
    int currentChapterNumber;
    std::string readingPlanId;
    int readingDay;
  };

  enum MenuItem : size_t {
    MODULE = 0,
    BOOK = 1,
    CHAPTER = 2,
    READING_PLAN = 3,
  };

  explicit BibleMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInputManager, Config config)
      : UiListActivity{"BibleMenu", renderer, mappedInputManager}, config_(std::move(config)) {}

  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  Config config_;
  std::array<freeink::ui::ListItem, 4> rowItems_{
      freeink::ui::ListItem{.label = "Select Module", .actionValue = 0},
      freeink::ui::ListItem{.label = "Select Book", .actionValue = 1},
      freeink::ui::ListItem{.label = "Select Chapter", .actionValue = 2},
      freeink::ui::ListItem{.label = "Open Reading Plan", .actionValue = 3},
  };

 protected:
  int listCount() const override { return static_cast<int>(rowItems_.size()); }
  void buildScreen(UiScreen& screen) override;
  void drawChrome() override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  void closeCancelled();
};
