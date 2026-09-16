#pragma once
#include <Connection.hpp>
#include <filesystem>
#include <set>
#include <span>
#include <vector>

#include "Statement.hpp"

namespace BibleToolbox {
using bookNumber = uint16_t;
using chapterNumber = uint8_t;
using verseNumber = uint8_t;

struct Module {
  std::string id;
  std::string description;
  std::string language;
  std::string chapterString;
  std::filesystem::path path;
};

struct Book {
  bookNumber number;
  std::string name;
  std::string alt;
  uint8_t chaptersCount;
  uint16_t prefixSum;
};

struct Verse {
  chapterNumber chapter;
  chapterNumber verse;
  std::string text;
};

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
  const Book* book;
  std::vector<VerseLocation> locations;
};

template <typename T>
concept BibleVersesProvider =
    requires(const T database, bookNumber book, chapterNumber chapter, bool excludeStrongsNumbers) {
      { database.chapterVerses(book, chapter, excludeStrongsNumbers) } -> std::same_as<std::vector<Verse> >;
      { database.operator[](book) } -> std::same_as<const Book*>;
    };

class Bible {
  std::vector<Book> books_;
  Connection connection_ = Connection();
  Statement chapterStatement_ = Statement();
  Module module_;

  [[nodiscard]] std::vector<Book> fetchBooks() const;

  [[nodiscard]] Module fetchInfo(const std::filesystem::path& path) const;

 public:
  explicit Bible(const std::filesystem::path& path, const char* vfs);

  ~Bible() = default;

  [[nodiscard]] std::span<const Book> books() const { return books_; }

  [[nodiscard]] std::string_view id() const { return module_.id; }

  [[nodiscard]] std::string_view chapterString() const { return module_.chapterString; }

  [[nodiscard]] std::string_view language() const { return module_.language; }

  const Book* operator[](bookNumber number) const;

  const Book* operator[](std::string_view name) const;

  [[nodiscard]] std::vector<Verse> chapterVerses(bookNumber book, chapterNumber chapter,
                                                 bool excludeStrongsNumbers) const;
};
}  // namespace BibleToolbox
