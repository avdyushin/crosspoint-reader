#pragma once

#include <vector>

#include "Location.h"

namespace BibleToolbox {
template <typename T>
concept DailyReadingProvider = requires(const T readingPlan, int day) {
  { readingPlan.locationsByDay(day) } -> std::same_as<std::vector<Location>>;
};
}  // namespace BibleToolbox
