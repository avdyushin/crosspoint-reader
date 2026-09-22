#pragma once

#include <string_view>
#include <vector>

#include "Constants.h"

namespace BibleToolbox {
struct VerseLocation {
  chapterNumber startChapter;
  verseNumber startVerse;
  chapterNumber endChapter;
  verseNumber endVerse;
};

struct UnresolvedReference {
  std::string_view name;
  std::vector<VerseLocation> locations;
};

struct Reference {
  const bookNumber book;
  std::vector<VerseLocation> locations;
};
}  // namespace BibleToolbox
