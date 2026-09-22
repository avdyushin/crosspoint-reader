#pragma once

#include "Constants.h"

namespace BibleToolbox {
struct Book {
  bookNumber number;
  std::string name;
  std::string alt;
  uint8_t chaptersCount;
  uint16_t prefixSum;
};
}  // namespace BibleToolbox
