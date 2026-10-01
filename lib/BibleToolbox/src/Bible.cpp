#include "Bible.h"

#include <algorithm>
#include <ranges>
#include <string_view>

#include "Connection.h"

namespace {
auto to_lower_view = [](std::string_view str) {
  return str | std::views::transform([](const unsigned char c) { return std::tolower(c); });
};

[[nodiscard]] bool case_insensitive_starts_with(const std::string_view str, const std::string_view prefix) {
  if (str.size() < prefix.size()) return false;
  return std::ranges::equal(to_lower_view(str.substr(0, prefix.size())), to_lower_view(prefix));
}

void process_verse_text_in_place(std::string& text, const bool exclude_strong_numbers) {
  size_t write_index = 0;
  size_t i = 0;
  const size_t length = text.size();

  while (i < length) {
    // 1. Handle Jesus Words Tag Transformations (<J> -> <i> and </J> -> </i>)
    if (i + 2 < length && text[i] == '<' && (text[i + 1] == 'J' || text[i + 1] == 'j') && text[i + 2] == '>') {
      text[write_index++] = '<';
      text[write_index++] = 'i';
      text[write_index++] = '>';
      i += 3;
      continue;
    }
    if (i + 3 < length && text[i] == '<' && text[i + 1] == '/' && (text[i + 2] == 'J' || text[i + 2] == 'j') &&
        text[i + 3] == '>') {
      text[write_index++] = '<';
      text[write_index++] = '/';
      text[write_index++] = 'i';
      text[write_index++] = '>';
      i += 4;
      continue;
    }

    // 2. Handle Strong's Number Blocks (<S>xxxx</S>)
    if (i + 2 < length && text[i] == '<' && (text[i + 1] == 'S' || text[i + 1] == 's') && text[i + 2] == '>') {
      if (exclude_strong_numbers) {
        i += 3;  // Skip opening "<S>"
        while (i < length && std::isdigit(static_cast<unsigned char>(text[i]))) {
          i++;
        }
        if (i + 3 < length && text[i] == '<' && text[i + 1] == '/' && (text[i + 2] == 'S' || text[i + 2] == 's') &&
            text[i + 3] == '>') {
          i += 4;
        }
        continue;
      }
    }

    // 3. Keep standard textual characters and collapse double spaces
    if (text[i] == ' ') {
      // Only write a space if the previous written character wasn't already a space
      if (write_index == 0 || text[write_index - 1] != ' ') {
        text[write_index++] = text[i];
      }
      i++;
    } else {
      text[write_index++] = text[i++];
    }
  }

  // 4. Shrink the string container to match the new length
  text.resize(write_index);
}
}  // namespace

namespace BibleToolbox {
Bible::Bible(const std::filesystem::path& path, const char* vfs) {
  if (connection_.open(path, vfs)) {
    constexpr auto verses_by_chapter = "SELECT verse, text FROM verses WHERE book_number = ? AND chapter = ?;";
    if (chapterStatement_.prepare(connection_.get(), verses_by_chapter)) {
      info_ = fetchInfo(path);
      books_ = fetchBooks();
    }

    constexpr auto verses_between =
        "SELECT verse, text FROM verses WHERE book_number = ? AND chapter = ? AND verse BETWEEN ? AND ?;";
    chapterVersesBetween_.prepare(connection_.get(), verses_between);

    constexpr auto verses_by_location = R"SQL(
        -- 1. Tail end of the start chapter
        SELECT chapter, verse, text FROM verses WHERE book_number = ? AND chapter = ? AND verse >= ?
        UNION ALL
        -- 2. Full chapters in between
        SELECT chapter, verse, text FROM verses WHERE book_number = ? AND chapter > ? AND chapter < ?
        UNION ALL
        -- 3. Head end of the final chapter
        SELECT chapter, verse, text FROM verses WHERE book_number = ? AND chapter = ? AND verse <= ?
        ORDER BY chapter, verse;
      )SQL";
    locationStatement_.prepare(connection_.get(), verses_by_location);
  }
}

const Book* Bible::operator[](const bookNumber number) const {
  if (const auto it = std::ranges::find(books_, number, &Book::number); it != books_.end()) {
    return &*it;
  }
  return nullptr;
}

const Book* Bible::operator[](const std::string_view name) const {
  const auto it = std::ranges::find_if(books_, [&name](const Book& book) {
    return case_insensitive_starts_with(book.name, name) || case_insensitive_starts_with(book.alt, name);
  });
  if (it != books_.end()) {
    return &*it;
  }
  return nullptr;
}

[[nodiscard]] std::vector<Book> Bible::fetchBooks() const {
  constexpr auto query = R"SQL(
                SELECT book_number, long_name, short_name, total_chapters
                FROM books
                ORDER BY book_number
            )SQL";
  auto statement = Statement();
  statement.prepare(connection_.get(), query);
  std::vector<Book> books;
  int prefix_sum = 0;
  while (statement.step() != SQLITE_DONE) {
    const int id = statement.getInt(0);
    const auto title = statement.getString(1);
    const auto alt = statement.getString(2);
    const int chapter_count = statement.getInt(3);
    const auto book = Book{.number = static_cast<bookNumber>(id),
                           .name = title,
                           .alt = alt,
                           .chaptersCount = static_cast<uint8_t>(chapter_count),
                           .prefixSum = static_cast<uint16_t>(prefix_sum)};
    prefix_sum += chapter_count;
    books.push_back(book);
  }
  return books;
}

BibleInfo Bible::fetchInfo(const std::filesystem::path& path) const {
  const auto id = path.stem();
  constexpr auto sql = "SELECT name, value FROM info";
  auto statement = Statement();
  statement.prepare(connection_.get(), sql);
  std::string description;
  std::string language;
  std::string chapter_string;
  while (statement.step() != SQLITE_DONE) {
    const auto name = statement.getStringView(0);
    const auto value = statement.getString(1);
    if (name == "description") {
      description = value;
    }
    if (name == "language") {
      language = value;
    }
    if (name == "chapter_string") {
      chapter_string = value;
    }
  }
  return {
      .id = std::string(id),
      .description = description,
      .language = language,
      .chapterString = chapter_string,
      .path = path,
  };
}

std::vector<Verse> Bible::versesInChapter(const bookNumber book, const chapterNumber chapter,
                                          const verseNumber startVerse, const verseNumber endVerse,
                                          const bool excludeStrongsNumbers) const {
  std::vector<Verse> verses;
  chapterVersesBetween_.reset();
  {
    auto _ = chapterVersesBetween_.bind(1, book);
  }
  {
    auto _ = chapterVersesBetween_.bind(2, chapter);
  }
  {
    auto _ = chapterVersesBetween_.bind(3, startVerse);
  }
  {
    auto _ = chapterVersesBetween_.bind(4, endVerse);
  }
  auto transform = [excludeStrongsNumbers](const std::string_view text) {
    std::string result{text};
    process_verse_text_in_place(result, excludeStrongsNumbers);
    return result;
  };
  while (chapterVersesBetween_.step() == SQLITE_ROW) {
    const auto verse = Verse{
        .book = book,
        .chapter = static_cast<chapterNumber>(chapter),
        .verse = static_cast<verseNumber>(chapterVersesBetween_.getInt(0)),
        .text = transform(chapterVersesBetween_.getString(1)),
    };
    verses.push_back(verse);
  }
  return verses;
}

std::vector<Verse> Bible::versesInChapter(const bookNumber book, const chapterNumber chapter,
                                          const bool excludeStrongsNumbers = true) const {
  std::vector<Verse> verses;
  chapterStatement_.reset();
  {
    auto _ = chapterStatement_.bind(1, book);
  }
  {
    auto _ = chapterStatement_.bind(2, chapter);
  }
  auto transform = [excludeStrongsNumbers](const std::string_view text) {
    std::string result{text};
    process_verse_text_in_place(result, excludeStrongsNumbers);
    return result;
  };
  while (chapterStatement_.step() == SQLITE_ROW) {
    const auto verse = Verse{
        .book = book,
        .chapter = static_cast<chapterNumber>(chapter),
        .verse = static_cast<verseNumber>(chapterStatement_.getInt(0)),
        .text = transform(chapterStatement_.getString(1)),
    };
    verses.push_back(verse);
  }
  return verses;
}

std::vector<Verse> Bible::versesByLocation(const Location& location, const bool excludeStrongsNumbers) const {
  if (location.range.startChapter > location.range.endChapter) {
    return {};
  }
  if (location.range.startChapter == location.range.endChapter) {
    if (location.range.startVerse > location.range.endVerse) {
      return {};
    }
    return versesInChapter(location.book, location.range.startChapter, location.range.startVerse,
                           location.range.endVerse, excludeStrongsNumbers);
  }
  std::vector<Verse> verses;
  locationStatement_.reset();
  {
    auto _ = locationStatement_.bind(1, location.book);
  }
  {
    auto _ = locationStatement_.bind(2, location.range.startChapter);
  }
  {
    auto _ = locationStatement_.bind(3, location.range.startVerse);
  }
  {
    auto _ = locationStatement_.bind(4, location.book);
  }
  {
    auto _ = locationStatement_.bind(5, location.range.startChapter);
  }
  {
    auto _ = locationStatement_.bind(6, location.range.endChapter);
  }
  {
    auto _ = locationStatement_.bind(7, location.book);
  }
  {
    auto _ = locationStatement_.bind(8, location.range.endChapter);
  }
  {
    auto _ = locationStatement_.bind(9, location.range.endVerse);
  }
  auto transform = [excludeStrongsNumbers](const std::string_view text) {
    std::string result{text};
    process_verse_text_in_place(result, excludeStrongsNumbers);
    return result;
  };
  while (locationStatement_.step() == SQLITE_ROW) {
    const auto verse = Verse{
        .book = location.book,
        .chapter = static_cast<chapterNumber>(locationStatement_.getInt(0)),
        .verse = static_cast<verseNumber>(locationStatement_.getInt(1)),
        .text = transform(locationStatement_.getString(2)),
    };
    verses.push_back(verse);
  }
  return verses;
}

std::string Bible::locationToString(const Location& location) const {
  const auto& [startChapter, startVerse, endChapter, endVerse] = location.range;

  std::string result = (*this)[location.book]->alt;
  result += ' ';
  result += std::to_string(startChapter);

  result += ':';
  result += std::to_string(startVerse);

  const bool chapterChanged = startChapter != endChapter;
  const bool verseChanged = startVerse != endVerse;

  if (chapterChanged || verseChanged) {
    result += '-';

    if (chapterChanged) {
      result += std::to_string(endChapter);
      result += ':';
      result += std::to_string(endVerse);
    } else {
      result += std::to_string(endVerse);
    }
  }

  return result;
}
}  // namespace BibleToolbox
