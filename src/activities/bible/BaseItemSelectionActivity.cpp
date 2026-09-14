#include "BaseItemSelectionActivity.h"

#include "BibleChapterSelectionActivity.h"
#include "BibleToolbox.h"
#include "components/UITheme.h"

template <HasName Item>
void BaseItemSelectionActivity<Item>::refreshWindow(const int start) {
  const int total = listCount();
  const int maxStart = std::max(0, total - WINDOW_SIZE);
  const int windowIndex = std::clamp(start, 0, maxStart);

  if (windowIndex == windowStart) {
    return;
  }

  windowCount = std::min(WINDOW_SIZE, total - windowIndex);
  for (int i = 0; i < windowCount; ++i) {
    const auto& items = items_[windowIndex + i];
    listLabels[i] = items.name;
    listItems[i] = freeink::ui::ListItem{
        .label = listLabels[i].c_str(),
        .actionValue = static_cast<uint16_t>(windowIndex + i),
    };
  }
  windowStart = windowIndex;
}

template <HasName Item>
void BaseItemSelectionActivity<Item>::activateIndex(const int index) {
  nav.selected = index;
  setResult(MenuResult{.action = index});
  finish();
}

template <HasName Item>
bool BaseItemSelectionActivity<Item>::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    result.data = MenuResult{.action = -1, .orientation = 0, .pageTurnOption = 0};
    setResult(std::move(result));
    finish();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateIndex(nav.selected);
    return true;
  }
  return false;
}

template <HasName Item>
void BaseItemSelectionActivity<Item>::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  // Content: the safe area minus the header band GUI.drawHeader paints.
  screen.setContentMarginFromScreen(freeink::ui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});

  freeink::ui::ListProps props{
      .count = static_cast<uint16_t>(listCount()),
      .action = ACTION_ROW,
      .inputMask = freeink::ui::InputTouch,
  };
  syncListViewport(screen, props);
  refreshWindow(nav.top);
  props.items = listItems;
  props.itemsWindowFirst = static_cast<uint16_t>(windowStart);
  screen.list(props);
}

template <HasName Item>
void BaseItemSelectionActivity<Item>::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);

  // Header via GUI.drawHeader (already FreeInkUI-themed) for the battery
  // indicator; the rest of the screen renders through the app.
  GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screen.width, metrics.headerHeight},
                 title_.c_str());
}

template class BaseItemSelectionActivity<BibleToolbox::Book>;
template class BaseItemSelectionActivity<BibleChapterInfo>;
