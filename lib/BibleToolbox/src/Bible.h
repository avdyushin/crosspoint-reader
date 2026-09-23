#pragma once
#include <filesystem>
#include <span>
#include <vector>

#include "BibleInfo.h"
#include "Book.h"
#include "Connection.h"
#include "Constants.h"
#include "Location.h"
#include "Statement.h"
#include "Verse.h"

namespace BibleToolbox {
class Bible {
  std::vector<Book> books_;
  Connection connection_ = Connection();
  Statement chapterStatement_ = Statement();
  Statement chapterVersesBetween_ = Statement();
  Statement locationStatement_ = Statement();
  BibleInfo info_;

  [[nodiscard]] std::vector<Book> fetchBooks() const;

  [[nodiscard]] BibleInfo fetchInfo(const std::filesystem::path& path) const;

  [[nodiscard]] std::vector<Verse> versesInChapter(bookNumber book, chapterNumber chapter, verseNumber startVerse,
                                                   verseNumber endVerse, bool excludeStrongsNumbers) const;

 public:
  explicit Bible(const std::filesystem::path& path, const char* vfs);

  ~Bible() = default;

  [[nodiscard]] std::span<const Book> books() const { return books_; }

  [[nodiscard]] std::string_view id() const { return info_.id; }

  [[nodiscard]] std::string_view chapterString() const { return info_.chapterString; }

  [[nodiscard]] std::string_view language() const { return info_.language; }

  [[nodiscard]] std::string_view description() const { return info_.description; }

  const Book* operator[](bookNumber number) const;

  const Book* operator[](std::string_view name) const;

  [[nodiscard]] std::vector<Verse> versesInChapter(bookNumber book, chapterNumber chapter,
                                                   bool excludeStrongsNumbers) const;

  [[nodiscard]] std::vector<Verse> versesByLocation(const Location& location, bool excludeStrongsNumbers) const;
};
}  // namespace BibleToolbox
