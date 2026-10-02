#pragma once
#include "BaseItemSelectionActivity.h"

struct DayInfo {
  std::string name;
};

class ReadingPlanDaySelectionActivity final : public BaseItemSelectionActivity<DayInfo> {
 public:
  explicit ReadingPlanDaySelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string_view title, const std::span<const DayInfo> days,
                                           const int currentDay)
      : BaseItemSelectionActivity("ReadingPlanDaySelection", renderer, mappedInput, title, days, currentDay) {}
};
