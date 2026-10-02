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
    int readingPlanDay;

    [[nodiscard]] bool hasModule() const { return !currentModuleId.empty(); }

    [[nodiscard]] std::string_view module() const { return hasModule() ? std::string_view{currentModuleId} : "None"; }

    [[nodiscard]] bool hasReadingPlan() const { return !readingPlanId.empty(); }

    [[nodiscard]] std::string_view readingPlan() const {
      return hasReadingPlan() ? std::string_view{readingPlanId} : "None";
    }
  };

  enum MenuItem : size_t {
    MODULE = 0,
    BOOK = 1,
    CHAPTER = 2,
    READING_PLAN = 3,
    DAILY_READING = 4,
  };

  explicit BibleMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInputManager, Config config)
      : UiListActivity{"BibleMenu", renderer, mappedInputManager}, config_(std::move(config)) {}

  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  Config config_;
  std::array<freeink::ui::ListItem, 5> rowItems_{
      freeink::ui::ListItem{.label = "Bible Module", .actionValue = 0},
      freeink::ui::ListItem{.label = "Current Book", .actionValue = 1},
      freeink::ui::ListItem{.label = "Current Chapter", .actionValue = 2},
      freeink::ui::ListItem{.label = "Reading Plan", .actionValue = 3},
      freeink::ui::ListItem{.label = "Open Daily Reading", .actionValue = 4},
  };

 protected:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void drawChrome() override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  void closeCancelled();
};
