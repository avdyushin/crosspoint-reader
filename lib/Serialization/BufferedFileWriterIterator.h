#pragma once

#include "BufferedFile.h"

namespace serialization {
class BufferedFileWriterIterator {
  BufferedFileWriter* writer_;

 public:
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;

  explicit constexpr BufferedFileWriterIterator(BufferedFileWriter& w) noexcept : writer_(&w) {}

  constexpr BufferedFileWriterIterator& operator*() noexcept { return *this; }

  constexpr BufferedFileWriterIterator& operator=(const char c) {
    writer_->write(&c, 1);
    return *this;
  }

  constexpr BufferedFileWriterIterator& operator++() noexcept { return *this; }
  constexpr BufferedFileWriterIterator operator++(int) const noexcept { return *this; }
};
}  // namespace serialization
