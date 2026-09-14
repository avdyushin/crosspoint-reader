#pragma once
#include "activities/UiListActivity.h"

class BibleMenuActivity final : public UiListActivity {
 public:
  enum MenuItem : size_t {
    MODULE = 0,
    BOOK = 1,
    CHAPTER = 2,
  };

  explicit BibleMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInputManager,
                             const std::string_view currentModuleId, const std::string_view currentBookIndex,
                             const int currentChapterNumber)
      : UiListActivity{"BibleMenu", renderer, mappedInputManager},
        currentModuleId_(currentModuleId),
        currentBookName_(currentBookIndex),
        currentChapterNumber_(currentChapterNumber) {}

  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  std::string_view currentModuleId_;
  std::string_view currentBookName_;
  int currentChapterNumber_;
  std::array<freeink::ui::ListItem, 3> rowItems_{freeink::ui::ListItem{.label = "Select Module", .actionValue = 0},
                                                 freeink::ui::ListItem{.label = "Select Book", .actionValue = 1},
                                                 freeink::ui::ListItem{.label = "Select Chapter", .actionValue = 2}};

 protected:
  int listCount() const override { return static_cast<int>(rowItems_.size()); }
  void buildScreen(UiScreen& screen) override;
  void drawChrome() override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  void closeCancelled();
};
