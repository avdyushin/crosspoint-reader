#include "BibleMenuActivity.h"

#include <format>

#include "components/UITheme.h"

void BibleMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  drawChrome();
  renderUi();
  drawFooter();
  renderer.displayBuffer();
}

int BibleMenuActivity::listCount() const {
  int count = 1;
  if (config_.hasModule()) {
    count += 3;
  }
  if (config_.hasReadingPlan()) {
    count += 1;
  }
  return count;
}

void BibleMenuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  // Content: the safe area minus the header band GUI.drawHeader paints.
  screen.setContentMarginFromScreen(
      freeink::ui::Insets{.top = static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                          .right = static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                          .bottom = static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                          .left = static_cast<int16_t>(safe.x)});

  // Info sub-header
  std::string subHeader;
  if (config_.currentModuleId.empty()) {
    subHeader = "No Bible module loaded";
  } else {
    subHeader = std::format("Reading: {} {}", config_.currentBookName, config_.currentChapterNumber);
  }
  const freeink::ui::Rect band = screen.takeTop(static_cast<int16_t>(metrics.tabBarHeight));
  const int16_t pad = screen.theme().headerSidePadding;
  screen.target().text(band.inset(freeink::ui::Insets{.top = 0, .right = pad, .bottom = 0, .left = pad}),
                       subHeader.c_str(), screen.theme().smallText);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  freeink::ui::ListProps listProps{
      .items = rowItems_.data(),
      .count = listCount(),
      .action = ACTION_ROW,
      .inputMask = freeink::ui::InputTouch,
      .labelText = screen.theme().smallText,
      .valueInset = 8,
  };
  listProps.labelText.maxLines = 2;

  rowItems_[MODULE].value = config_.module().data();

  if (config_.hasModule()) {
    chapterValue_ = std::to_string(config_.currentChapterNumber);
    rowItems_[BOOK].value = config_.currentBookName.data();
    rowItems_[CHAPTER].value = chapterValue_.data();
    rowItems_[READING_PLAN].value = config_.readingPlan().data();
  }

  if (config_.hasReadingPlan()) {
    dayValue_ = std::format("Day {}", config_.readingPlanDay);
    rowItems_[DAILY_READING].value = dayValue_.data();
  }

  syncListViewport(screen, listProps);
  screen.list(listProps);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (activeNav().selected == MODULE) {
    auto style = screen.theme().smallText;
    style.maxLines = 2;
    const int16_t lineHeight = screen.target().lineHeight(style.font);
    constexpr int16_t bottomGap = 8;
    const freeink::ui::Rect desc = screen.takeBottom(static_cast<int16_t>(2 * lineHeight + bottomGap), 0);
    const auto rect = desc.inset(freeink::ui::Insets{.top = 0, .right = pad, .bottom = bottomGap, .left = pad});
    screen.target().text(rect, config_.moduleDescription.c_str(), style);
  }
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
