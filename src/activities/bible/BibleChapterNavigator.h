#pragma once
#include "BibleToolbox.h"

class BibleChapterNavigator {
 public:
  struct NavFirstPage {};
  struct NavLastPage {};
  struct NavTargetPage {
    int page;
  };

  using NavDirection = std::variant<NavTargetPage, NavFirstPage, NavLastPage>;

  int totalPages = 0;
  int currentPage = 0;
  int inBookChapter = 1;
  int currentBookIndex = 10;
  std::span<const BibleToolbox::Book> books;
  std::function<void(int, int, NavDirection)> onChapterChanged;

  [[nodiscard]] float progress() const {
    if (totalPages == 0) {
      return 0.f;
    }
    const auto globalChapter = currentBook()->prefixSum + (inBookChapter - 1);
    const auto thisChapter = static_cast<float>(currentPage) / static_cast<float>(totalPages);
    return (static_cast<float>(globalChapter) + thisChapter) * 100.f / static_cast<float>(totalChapters());
    // return totalPages > 0 ? (static_cast<float>(currentPage) + 1) * 100.f / static_cast<float>(totalPages) : 0;
  }

  [[nodiscard]] const BibleToolbox::Book* currentBook() const { return &books[currentBookIndex]; }

  [[nodiscard]] int currentBookNumber() const { return currentBook()->number; }

  bool skipPages(const int amount) {
    const int target_page = std::clamp(currentPage + amount, 0, totalPages - 1);
    if (currentPage != target_page) {
      currentPage = target_page;
      return true;
    }
    return false;
  }

  bool nextPageOrChapter() {
    if (currentPage < totalPages - 1) {
      currentPage++;
      return true;
    }
    checkNextChapter();
    return false;
  }

  bool previousPageOrChapter() {
    if (currentPage > 0) {
      currentPage--;
      return true;
    }
    checkPreviousChapter();
    return false;
  }

 private:
  void checkNextChapter() {
    if (inBookChapter + 1 < totalChaptersInBook()) {
      inBookChapter++;
      onChapterChanged(currentBookIndex, inBookChapter, NavFirstPage{});
    } else if (currentBookIndex + 1 < books.size()) {
      currentBookIndex++;
      inBookChapter = 1;
      onChapterChanged(currentBookIndex, inBookChapter, NavFirstPage{});
    }
  }

  void checkPreviousChapter() {
    if (inBookChapter > 1) {
      inBookChapter--;
      onChapterChanged(currentBookIndex, inBookChapter, NavLastPage{});
    } else if (currentBookIndex > 0) {
      currentBookIndex--;
      inBookChapter = totalChaptersInBook() - 1;
      onChapterChanged(currentBookIndex, inBookChapter, NavLastPage{});
    }
  }

  [[nodiscard]] int totalChaptersInBook() const { return currentBook()->chaptersCount; }

  [[nodiscard]] int totalChapters() const { return books.back().prefixSum + books.back().chaptersCount; }
};
