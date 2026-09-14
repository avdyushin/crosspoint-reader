#pragma once
#include "BaseItemSelectionActivity.h"

struct BibleChapterInfo {
  std::string name;
};

class BibleChapterSelectionActivity final : public BaseItemSelectionActivity<BibleChapterInfo> {
 public:
  explicit BibleChapterSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::string_view title, const std::span<const BibleChapterInfo> chapters,
                                         const int currentChapterNumber)
      : BaseItemSelectionActivity("BibleChapterSelection", renderer, mappedInput, title, chapters,
                                  currentChapterNumber) {}
};
