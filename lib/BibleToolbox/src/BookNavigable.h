#pragma once

#include <concepts>
#include <string_view>

namespace BibleToolbox {
template <typename T>
concept BookNavigable = requires(const T& navigator, int index, int chapter) {
  { navigator.bookName(index) } -> std::convertible_to<std::string_view>;
  { navigator.bookNumber(index) } -> std::convertible_to<int>;
  { navigator.totalBooks() } -> std::convertible_to<int>;
  { navigator.chaptersCount(index) } -> std::convertible_to<int>;
  { navigator.globalChapter(index, chapter) } -> std::convertible_to<int>;
  { navigator.totalChapters() } -> std::convertible_to<int>;
};
}  // namespace BibleToolbox
