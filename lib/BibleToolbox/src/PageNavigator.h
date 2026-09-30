#pragma once

#include <algorithm>
#include <functional>
#include <variant>

#include "Bible.h"
#include "BookNavigable.h"
#include "BookPosition.h"

namespace BibleToolbox {
template <BookNavigable T>
class PageNavigator {
  T& navigator_;

 public:
  using Callback =
      std::function<void(const BookPosition& oldPosition, const BookPosition& newPosition, PositionChange changes)>;

  int totalPages = 0;
  Callback callback;

  explicit PageNavigator(T& navigator) : navigator_(navigator) {}

  [[nodiscard]] int getPage() const { return position_.page; }
  bool setPage(const int page) {
    return setPosition(BookPosition{.book = position_.book, .chapter = position_.chapter, .page = page});
  }

  [[nodiscard]] int getBook() const { return position_.book; }
  bool setBook(const int book) {
    return setPosition(BookPosition{.book = book, .chapter = position_.chapter, .page = position_.page});
  }

  [[nodiscard]] int getChapter() const { return position_.chapter; }
  bool setChapter(const int chapter) {
    return setPosition(BookPosition{.book = position_.book, .chapter = chapter, .page = position_.page});
  }

  [[nodiscard]] float progress() const {
    if (totalPages == 0) {
      return 0.f;
    }
    const auto thisChapter = static_cast<float>(position_.page) / static_cast<float>(totalPages);
    return (static_cast<float>(navigator_.globalChapter(position_.book, position_.chapter)) + thisChapter) * 100.f /
           static_cast<float>(navigator_.totalChapters());
  }

  [[nodiscard]] std::string_view currentBookName() const { return navigator_.bookName(position_.book); }

  [[nodiscard]] int currentBookNumber() const { return navigator_.bookNumber(position_.book); }

  [[nodiscard]] int chapterCount() const { return navigator_.chaptersCount(position_.book); }

  bool skipPages(const int amount) {
    const int target_page = std::clamp(position_.page + amount, 0, totalPages - 1);
    return setPosition(BookPosition{.book = position_.book, .chapter = position_.chapter, .page = target_page});
  }

  bool turnPage(const bool isForward) {
    if (isForward) {
      return nextPageOrChapter();
    }
    return previousPageOrChapter();
  }

  bool setPosition(const BookPosition newPosition) {
    if (!validatePosition(newPosition)) {
      return false;
    }

    const auto oldPosition = position_;

    if (const auto changes = changedFields(oldPosition, newPosition); changes != PositionChange::None) {
      position_ = newPosition;
      if (callback) {
        callback(oldPosition, newPosition, changes);
      }
      return true;
    }
    return false;
  }

 private:
  static PositionChange changedFields(const BookPosition& oldPosition, const BookPosition& newPosition) {
    auto changes = PositionChange::None;

    if (oldPosition.book != newPosition.book) {
      changes = changes | PositionChange::Book;
    }

    if (oldPosition.chapter != newPosition.chapter) {
      changes = changes | PositionChange::Chapter;
    }

    if (oldPosition.page != newPosition.page) {
      changes = changes | PositionChange::Page;
    }

    return changes;
  }

  bool validatePosition(const BookPosition& position) {
    if (position.book < 0 || position.book >= navigator_.totalBooks()) {
      return false;
    }
    if (position.chapter < START_CHAPTER_NUMBER || position.chapter > navigator_.chaptersCount(position.book)) {
      return false;
    }
    if (position.page < 0 || (totalPages != 0 && position.page >= totalPages)) {
      return false;
    }
    return true;
  }

  bool nextPageOrChapter() {
    if (!setPage(position_.page + 1)) {
      checkNextChapter();
      return false;
    }
    return true;
  }

  bool previousPageOrChapter() {
    if (!setPage(position_.page - 1)) {
      checkPreviousChapter();
      return false;
    }
    return true;
  }

  void checkNextChapter() {
    if (position_.chapter + 1 <= navigator_.chaptersCount(position_.book)) {
      setPosition(BookPosition{.book = position_.book, .chapter = position_.chapter + 1, .page = 0});
    } else if (position_.book + 1 < navigator_.totalBooks()) {
      setPosition(BookPosition{.book = position_.book + 1, .chapter = START_CHAPTER_NUMBER, .page = 0});
    }
  }

  void checkPreviousChapter() {
    if (position_.chapter > START_CHAPTER_NUMBER) {
      setPosition(BookPosition{.book = position_.book, .chapter = position_.chapter - 1, .page = 0});
    } else if (position_.book > 0) {
      auto lastChapter = navigator_.chaptersCount(position_.book - 1);
      setPosition(BookPosition{.book = position_.book - 1, .chapter = lastChapter, .page = 0});
    }
  }

  BookPosition position_{};
};
}  // namespace BibleToolbox
