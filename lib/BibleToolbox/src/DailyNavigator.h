#pragma once

namespace BibleToolbox {

class DailyNavigator {
  int daysCount_{};
  std::string title_{};

 public:
  explicit DailyNavigator() = default;

  void configureWith(const std::string& title, const int daysCount) {
    title_ = title;
    daysCount_ = daysCount;
  }

  [[nodiscard]] std::string_view bookName(const int) const { return title_; }
  static int bookNumber(const int index) { return index; }
  static int totalBooks() { return 1; }
  [[nodiscard]] int chaptersCount(const int) const { return daysCount_; }
  static int globalChapter(const int, const int chapter) { return chapter; }
  [[nodiscard]] int totalChapters() const { return daysCount_; }
};
}  // namespace BibleToolbox
