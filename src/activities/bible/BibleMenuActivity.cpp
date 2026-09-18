#include "BibleMenuActivity.h"

#include "BibleActivity.h"
#include "components/UITheme.h"

namespace {}

void BibleMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  drawChrome();
  renderUi();
  drawFooter();
  renderer.displayBuffer();
}

void BibleMenuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  // Content: the safe area minus the header band GUI.drawHeader paints.
  screen.setContentMarginFromScreen(freeink::ui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});

  freeink::ui::ListProps props{
      .items = &rowItems_[0],
      .count = static_cast<uint16_t>(rowItems_.size()),
      .action = ACTION_ROW,
      .inputMask = freeink::ui::InputTouch,
      .labelText = screen.theme().smallText,
      .valueInset = 8,
  };
  props.labelText.maxLines = 2;

  const std::string chapter = std::to_string(currentChapterNumber_);
  // const auto bible = BibleContext::activeBible.load(std::memory_order_acquire);
  // const std::string_view book = bible->books()[currentBookName_].name;

  rowItems_[MODULE].value = currentModuleId_.data();
  rowItems_[BOOK].value = currentBookName_.data();
  rowItems_[CHAPTER].value = chapter.c_str();

  syncListViewport(screen, props);
  screen.list(props);
}

void BibleMenuActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);

  // Header via GUI.drawHeader (already FreeInkUI-themed) for the battery
  // indicator; the rest of the screen renders through the app.
  GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screen.width, metrics.headerHeight},
                 "Bible Menu");
}

void BibleMenuActivity::activateIndex(const int index) {
  setResult(MenuResult{.action = index});
  finish();
}

bool BibleMenuActivity::handleCustomInput() { return false; }

bool BibleMenuActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closeCancelled();
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateIndex(nav.selected);
    return true;
  }

  return false;
}

bool BibleMenuActivity::handleHomeGesture() {
  closeCancelled();
  return true;
}

void BibleMenuActivity::closeCancelled() {
  ActivityResult result;
  result.isCancelled = true;
  result.data = MenuResult{.action = -1, .orientation = 0, .pageTurnOption = 0};
  setResult(std::move(result));
  finish();
}
