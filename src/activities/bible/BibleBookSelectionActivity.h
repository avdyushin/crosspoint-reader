#pragma once
#include "BaseItemSelectionActivity.h"
#include "Bible.h"

class BibleBookSelectionActivity final : public BaseItemSelectionActivity<BibleToolbox::Book> {
 public:
  explicit BibleBookSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                      const std::string_view title, const std::span<const BibleToolbox::Book> books,
                                      const int currentBookIndex)
      : BaseItemSelectionActivity("BibleBookSelection", renderer, mappedInput, title, books, currentBookIndex) {}
};
