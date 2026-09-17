#pragma once

#include <algorithm>
#include <functional>
#include <variant>

#include "BibleToolbox.h"

namespace BibleToolbox {
class ChapterNavigator {
  int currentPage_ = 0;

 public:
  struct NavFirstPage {};
  struct NavLastPage {};
  struct NavTargetPage {
    int page;
  };

  using NavDirection = std::variant<NavTargetPage, NavFirstPage, NavLastPage>;

  int totalPages = 0;
  int inBookChapter = START_CHAPTER_NUMBER;
  int currentBookIndex = 0;
  std::span<const Book> books;
  std::function<void(int, int, NavDirection)> onChapterChanged;
  std::function<void(int)> onPageChanged;

  [[nodiscard]] int getCurrentPage() const { return currentPage_; }
  void setCurrentPage(const int currentPage) {
    if (totalPages > 0) {
      currentPage_ = std::clamp(currentPage, 0, totalPages - 1);
      onPageChanged(currentPage_);
    }
  }

  [[nodiscard]] float progress() const {
    if (totalPages == 0) {
      return 0.f;
    }
    const auto globalChapter = currentBook()->prefixSum + (inBookChapter - 1);
    const auto thisChapter = static_cast<float>(currentPage_) / static_cast<float>(totalPages);
    return (static_cast<float>(globalChapter) + thisChapter) * 100.f / static_cast<float>(totalChapters());
  }

  [[nodiscard]] const Book* currentBook() const { return &books[currentBookIndex]; }

  [[nodiscard]] int currentBookNumber() const { return currentBook()->number; }

  bool skipPages(const int amount) {
    const int target_page = std::clamp(currentPage_ + amount, 0, totalPages - 1);
    if (currentPage_ != target_page) {
      setCurrentPage(target_page);
      return true;
    }
    return false;
  }

  bool turnPage(const bool isForward) {
    if (isForward) {
      return nextPageOrChapter();
    }
    return previousPageOrChapter();
  }

 private:
  bool nextPageOrChapter() {
    if (currentPage_ < totalPages - 1) {
      setCurrentPage(currentPage_ + 1);
      return true;
    }
    checkNextChapter();
    return false;
  }

  bool previousPageOrChapter() {
    if (currentPage_ > 0) {
      setCurrentPage(currentPage_ - 1);
      return true;
    }
    checkPreviousChapter();
    return false;
  }

  void checkNextChapter() {
    if (inBookChapter + 1 <= totalChaptersInBook()) {
      inBookChapter++;
      onChapterChanged(currentBookIndex, inBookChapter, NavFirstPage{});
    } else if (currentBookIndex + 1 < books.size()) {
      currentBookIndex++;
      inBookChapter = START_CHAPTER_NUMBER;
      onChapterChanged(currentBookIndex, inBookChapter, NavFirstPage{});
    }
  }

  void checkPreviousChapter() {
    if (inBookChapter > START_CHAPTER_NUMBER) {
      inBookChapter--;
      onChapterChanged(currentBookIndex, inBookChapter, NavLastPage{});
    } else if (currentBookIndex > 0) {
      currentBookIndex--;
      inBookChapter = totalChaptersInBook();
      onChapterChanged(currentBookIndex, inBookChapter, NavLastPage{});
    }
  }

  [[nodiscard]] int totalChaptersInBook() const { return currentBook()->chaptersCount; }

  [[nodiscard]] int totalChapters() const { return books.back().prefixSum + books.back().chaptersCount; }
};
}  // namespace BibleToolbox
