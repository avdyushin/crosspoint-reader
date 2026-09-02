#pragma once

#include "activities/Activity.h"

class BibleActivity : public Activity {
 protected:
  bool handleBackNavigation();

 public:
  BibleActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~BibleActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  //   bool isBibleActivity() const final { return true; }
  bool handleForcedRefresh() final;
};
