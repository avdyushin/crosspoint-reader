#pragma once

#include <vector>

#include "Book.h"
#include "Constants.h"
#include "Location.h"
#include "Verse.h"

namespace BibleToolbox {
template <typename T>
concept VersesProvider =
    requires(const T database, bookNumber book, chapterNumber chapter, Location& location, bool excludeStrongsNumbers) {
      { database.versesInChapter(book, chapter, excludeStrongsNumbers) } -> std::same_as<std::vector<Verse>>;
      { database.versesByLocation(location, excludeStrongsNumbers) } -> std::same_as<std::vector<Verse>>;
      { database.operator[](book) } -> std::same_as<const Book*>;
    };
}  // namespace BibleToolbox
