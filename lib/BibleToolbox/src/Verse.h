#pragma once

#include "Constants.h"

namespace BibleToolbox {
struct Verse {
  bookNumber book;
  chapterNumber chapter;
  chapterNumber verse;
  std::string text;
};
}  // namespace BibleToolbox
