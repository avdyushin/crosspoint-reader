#pragma once

#include <algorithm>
#include <functional>
#include <variant>

#include "Bible.h"
#include "BookNavigable.h"

namespace BibleToolbox {
struct NavFirstPage {};
struct NavLastPage {};
struct NavTargetPage {
  int page;
};

using NavDirection = std::variant<NavTargetPage, NavFirstPage, NavLastPage>;

template <BookNavigable T>
class PageNavigator {
  int currentPage_ = 0;
  T& navigator_;

 public:
  int totalPages = 0;
  int inBookChapter = START_CHAPTER_NUMBER;
  int currentBookIndex = 0;
  std::function<void(int, int, NavDirection)> onChapterChanged;
  std::function<void(int)> onPageChanged;

  explicit PageNavigator(T& navigator) : navigator_(navigator) {}

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
    const auto thisChapter = static_cast<float>(getCurrentPage()) / static_cast<float>(totalPages);
    return (static_cast<float>(navigator_.globalChapter(currentBookIndex, inBookChapter)) + thisChapter) * 100.f /
           static_cast<float>(navigator_.totalChapters());
  }

  [[nodiscard]] std::string_view currentBookName() const { return navigator_.bookName(currentBookIndex); }

  [[nodiscard]] int currentBookNumber() const { return navigator_.bookNumber(currentBookIndex); }

  [[nodiscard]] int totalBooks() const { return navigator_.totalBooks(); }

  [[nodiscard]] int totalChapters() const { return navigator_.totalChapters(); }

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
    if (inBookChapter + 1 <= navigator_.chaptersCount(currentBookIndex)) {
      ++inBookChapter;
      onChapterChanged(currentBookIndex, inBookChapter, NavFirstPage{});
    } else if (currentBookIndex + 1 < navigator_.totalBooks()) {
      ++currentBookIndex;
      inBookChapter = START_CHAPTER_NUMBER;
      onChapterChanged(currentBookIndex, inBookChapter, NavFirstPage{});
    }
  }

  void checkPreviousChapter() {
    if (inBookChapter > START_CHAPTER_NUMBER) {
      --inBookChapter;
      onChapterChanged(currentBookIndex, inBookChapter, NavLastPage{});
    } else if (currentBookIndex > 0) {
      --currentBookIndex;
      inBookChapter = navigator_.chaptersCount(currentBookIndex);
      onChapterChanged(currentBookIndex, inBookChapter, NavLastPage{});
    }
  }
};
}  // namespace BibleToolbox
