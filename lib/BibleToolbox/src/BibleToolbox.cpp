#include "BibleToolbox.h"

#include <algorithm>
#include <ranges>
#include <string_view>

#include "Connection.hpp"

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
Bible::Bible(const std::filesystem::path& path) {
  if (connection_.open(path)) {
    constexpr auto verses_by_chapter = "SELECT verse, text FROM verses WHERE book_number = ? AND chapter = ?;";
    if (chapterStatement_.prepare(connection_.get(), verses_by_chapter)) {
      module_ = fetchInfo(path);
      books_ = fetchBooks();
    }
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
    const int id = statement.get_int(0);
    const auto title = statement.get_string(1);
    const auto alt = statement.get_string(2);
    const int chapter_count = statement.get_int(3);
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

Module Bible::fetchInfo(const std::filesystem::path& path) const {
  const auto id = path.stem();
  constexpr auto sql = "SELECT name, value FROM info";
  auto statement = Statement();
  statement.prepare(connection_.get(), sql);
  std::string description;
  std::string language;
  std::string chapter_string;
  while (statement.step() != SQLITE_DONE) {
    const auto name = statement.get_string_view(0);
    const auto value = statement.get_string(1);
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

std::vector<Verse> Bible::chapterVerses(const bookNumber book, const chapterNumber chapter,
                                        const bool excludeStrongsNumbers = true) const {
  std::vector<Verse> verses;
  chapterStatement_.reset();
  auto _ = chapterStatement_.bind(1, book);
  auto _ = chapterStatement_.bind(2, chapter);
  auto transform = [excludeStrongsNumbers](const std::string_view text) {
    std::string result{text};
    process_verse_text_in_place(result, excludeStrongsNumbers);
    return result;
  };
  while (chapterStatement_.step() == SQLITE_ROW) {
    const auto verse = Verse{
        .chapter = static_cast<chapterNumber>(chapter),
        .verse = static_cast<chapterNumber>(chapterStatement_.get_int(0)),
        .text = transform(chapterStatement_.get_string(1)),
    };
    verses.push_back(verse);
  }
  return verses;
}
}  // namespace BibleToolbox
