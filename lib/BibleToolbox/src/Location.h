#pragma once

#include "Constants.h"

namespace BibleToolbox {
struct VersesRange {
  chapterNumber startChapter;
  verseNumber startVerse;
  chapterNumber endChapter;
  verseNumber endVerse;
};

struct Location {
  bookNumber book;
  VersesRange range;
};
}  // namespace BibleToolbox
