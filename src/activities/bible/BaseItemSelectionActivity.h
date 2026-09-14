#pragma once
#include <span>

#include "activities/UiListActivity.h"

template <typename T>
concept HasName = requires(const T object) {
  object.name;
  { object.name } -> std::convertible_to<std::string_view>;
};

template <HasName Item>
class BaseItemSelectionActivity : public UiListActivity {
  std::span<const Item> items_;
  int currentItemIndex_;
  std::string title_;

  static constexpr auto WINDOW_SIZE = 24;
  std::string listLabels[WINDOW_SIZE];
  freeink::ui::ListItem listItems[WINDOW_SIZE];
  int windowStart = -1;
  int windowCount = 0;
  void refreshWindow(int start);

 protected:
  int listCount() const override { return static_cast<int>(items_.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleButtons() override;
  void drawChrome() override;

 public:
  explicit BaseItemSelectionActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     const std::string_view title, const std::span<const Item> items,
                                     const int currentItemIndex)
      : UiListActivity(name, renderer, mappedInput),
        items_(items),
        currentItemIndex_(currentItemIndex),
        title_(title) {}
};
