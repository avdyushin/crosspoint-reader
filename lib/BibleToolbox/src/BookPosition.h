#pragma once

#include <cstdint>

namespace BibleToolbox {
struct BookPosition {
  int book{};
  int chapter{};
  int page{};

  friend bool operator==(const BookPosition&, const BookPosition&) = default;
};

enum class PositionChange : std::uint8_t { None = 0, Book = 1 << 0, Chapter = 1 << 1, Page = 1 << 2 };

constexpr PositionChange operator|(PositionChange lhs, PositionChange rhs) {
  return static_cast<PositionChange>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr bool hasChange(PositionChange value, PositionChange flag) {
  return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

}  // namespace BibleToolbox
