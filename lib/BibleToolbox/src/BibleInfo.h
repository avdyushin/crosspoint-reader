#pragma once

#include <filesystem>

namespace BibleToolbox {
struct BibleInfo {
  std::string id;
  std::string description;
  std::string language;
  std::string chapterString;
  std::filesystem::path path;
};
}  // namespace BibleToolbox
