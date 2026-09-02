#include "BibleActivity.h"

#include <I18n.h>
#include <sqlite3.h>

#include "../reader/ReaderUtils.h"
#include "fontIds.h"

BibleActivity::BibleActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity(name, renderer, mappedInput) {}

void BibleActivity::onEnter() {
  Activity::onEnter();

  requestUpdate();
}

void BibleActivity::onExit() { Activity::onExit(); }

bool BibleActivity::handleBackNavigation() {
  return ReaderUtils::handleBackNavigation(mappedInput, activityManager, "path?",
                                           {this, [](void* ctx) { static_cast<BibleActivity*>(ctx)->onGoHome(); }});
}

void BibleActivity::loop() {
  if (handleBackNavigation()) return;

  const auto touch = ReaderUtils::detectTouchPageTurn(renderer, mappedInput);
  auto [prevTriggered, nextTriggered, fromTilt] = ReaderUtils::detectPageTurn(mappedInput);
  prevTriggered = prevTriggered || touch.prev;
  nextTriggered = nextTriggered || touch.next;

  if (!prevTriggered && !nextTriggered) return;

  requestUpdate();
}

void BibleActivity::render(RenderLock&& lock) {
  renderer.clearScreen();
  renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
  renderer.displayBuffer();
}

bool BibleActivity::handleForcedRefresh() {
  {
    RenderLock lock(*this);
    // pagesUntilFullRefresh = 1;
    // forcedRefreshPending = true;
  }
  requestUpdate();
  return true;
}
