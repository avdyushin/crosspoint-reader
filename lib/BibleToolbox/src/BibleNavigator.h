#pragma once

#include "Bible.h"

namespace BibleToolbox {

class BibleNavigator {
  std::span<const Book> books_{};

  [[nodiscard]] const Book* operator[](const int index) const { return &books_[index]; }

 public:
  explicit BibleNavigator() = default;

  void configureWith(const std::span<const Book> books) { books_ = books; }

  [[nodiscard]] std::string_view bookName(const int index) const { return (*this)[index]->name; }
  [[nodiscard]] int bookNumber(const int index) const { return (*this)[index]->number; }
  [[nodiscard]] int totalBooks() const { return static_cast<int>(books_.size()); }
  [[nodiscard]] int chaptersCount(const int index) const { return (*this)[index]->chaptersCount; }
  [[nodiscard]] int globalChapter(const int index, const int chapter) const {
    return (*this)[index]->prefixSum + chapter - 1;
  }
  [[nodiscard]] int totalChapters() const { return books_.back().prefixSum + books_.back().chaptersCount; }
};
}  // namespace BibleToolbox
