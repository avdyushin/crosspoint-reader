#pragma once
#include <filesystem>

namespace BibleToolbox {
struct ReadingPlanInfo {
  std::string id;
  std::string description;
  std::filesystem::path path;
  int daysCount;
};
}  // namespace BibleToolbox
