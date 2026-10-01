#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "Bible.h"
#include "ReadingPlan.h"
#include "util/BibleVerseFormatter.h"

using namespace BibleToolbox;

constexpr auto GENESIS_BOOK_NUMBER = 10;

namespace {
class MockVersesProvider {
  const Book book_ = Book{.name = "Genesis"};

 public:
  const Book* operator[](bookNumber) const { return &book_; }
  static std::vector<Verse> versesInChapter(bookNumber, chapterNumber, bool) {
    return std::vector{
        Verse{.chapter = 1, .verse = 1, .text = "First verse."},
        Verse{.chapter = 1, .verse = 2, .text = "Last verse."},
    };
  }
  static std::vector<Verse> versesByLocation(const Location& location, const bool exclude) {
    const auto count = location.range.startVerse - location.range.endVerse + 1;
    const auto result = versesInChapter(location.book, location.range.startChapter, exclude);
    return {
        result.begin(),
        result.begin() + count,
    };
  }
  static std::string locationToString(const Location& location) { return "Gen. 1:1"; }
};

class MockPlanProvider {
 public:
  static std::vector<Location> locationsByDay(const int day) {
    return {
        Location{
            .book = GENESIS_BOOK_NUMBER,
            .range = {.startChapter = 1, .startVerse = 1, .endChapter = 1, .endVerse = 1},
        },
    };
  }
};
}  // namespace

class BibleToolboxTest : public ::testing::Test {
 protected:
  std::filesystem::path bible_path;
  std::unique_ptr<Bible> bible;

  std::filesystem::path plan_path;
  std::unique_ptr<ReadingPlan> plan;

  void SetUp() override {
    bible_path = std::filesystem::path(__FILE__).parent_path() / "assets" / "KJV+.SQLite3";
    bible = std::make_unique<Bible>(bible_path, nullptr);
    plan_path = std::filesystem::path(__FILE__).parent_path() / "assets" / "OY-p.plan.SQLite3";
    plan = std::make_unique<ReadingPlan>(plan_path, nullptr);
  }
};

TEST_F(BibleToolboxTest, HtmlVerseFormatterFirstChapter) {
  std::string actual;
  const auto provider = MockVersesProvider{};
  constexpr auto formatter = BibleVerseFormatter{};
  formatter.formatChapter(std::back_inserter(actual), provider, 0, 1, "Ch.");
  constexpr auto expected =
      "<html><body>\n"
      "<h1>Genesis</h1>\n"
      "<h2>Ch. 1</h2>\n"
      "<p><sup>1 </sup> First verse.</p>\n"
      "<p><sup>2 </sup> Last verse.</p>\n"
      "</body></html>\n";

  ASSERT_EQ(expected, actual) << "Invalid formatted text";
}

TEST_F(BibleToolboxTest, HtmlVerseFormatterNonFirstChapter) {
  std::string actual;
  const auto provider = MockVersesProvider{};
  constexpr auto formatter = BibleVerseFormatter{};
  formatter.formatChapter(std::back_inserter(actual), provider, 0, 2, "Ch.");
  constexpr auto expected =
      "<html><body>\n"
      "<h2>Ch. 2</h2>\n"
      "<p><sup>1 </sup> First verse.</p>\n"
      "<p><sup>2 </sup> Last verse.</p>\n"
      "</body></html>\n";

  ASSERT_EQ(expected, actual) << "Invalid formatted text";
}

TEST_F(BibleToolboxTest, HtmlReadingDayFormatter) {
  std::string actual;
  const auto provider = MockVersesProvider{};
  constexpr auto plan = MockPlanProvider{};
  constexpr auto formatter = BibleVerseFormatter{};
  formatter.readingDayVerses(std::back_inserter(actual), plan, provider, 0, "Day");
  constexpr auto expected =
      "<html><body>\n"
      "<h1>Day 0</h1>\n"
      "<h2>Gen. 1:1</h2>\n"
      "<p><sup>1:1 </sup> First verse.</p>\n"
      "</body></html>\n";
  ASSERT_EQ(expected, actual) << "Invalid formatted text";
}

TEST_F(BibleToolboxTest, ValidatesBookAndChapterCounts) {
  constexpr auto TOTAL_BOOKS = 66;
  constexpr auto TOTAL_CHAPTERS = 1189;

  ASSERT_EQ(bible->books().size(), TOTAL_BOOKS);

  const auto last_book = bible->books().back();
  EXPECT_EQ(last_book.prefixSum + last_book.chaptersCount, TOTAL_CHAPTERS) << "Invalid global number of chapters";
}

TEST_F(BibleToolboxTest, HandlesBookLookupsAndBounds) {
  constexpr auto GENESIS_TOTAL_CHAPTERS = 50;

  const auto genesis_by_id = (*bible)[GENESIS_BOOK_NUMBER];
  ASSERT_NE(genesis_by_id, nullptr) << "Invalid book id: " << GENESIS_BOOK_NUMBER;
  EXPECT_EQ(genesis_by_id->chaptersCount, GENESIS_TOTAL_CHAPTERS)
      << "Invalid book chapters count in book " << genesis_by_id->name;

  EXPECT_EQ((*bible)[999], nullptr) << "Expected nullptr for non-existent book ID 999";

  const auto book_by_code = (*bible)["gen"];
  ASSERT_NE(book_by_code, nullptr) << "Can't find book 'gen'";
  EXPECT_EQ(book_by_code->name, "Genesis");
}

TEST_F(BibleToolboxTest, ExtractsVersesWithStrongsTags) {
  constexpr auto GENESIS_BOOK_NUMBER = 10;
  constexpr auto GENESIS_FIRST_CHAPTER_VERSES_COUNT = 31;
  constexpr auto first_verse_text =
      "In the beginning<S>7225</S> God<S>430</S> created<S>1254</S> <S>853</S> the heaven<S>8064</S> and<S>853</S> the "
      "earth<S>776</S>.";

  const auto verses = bible->versesInChapter(GENESIS_BOOK_NUMBER, 1, false);
  ASSERT_EQ(verses.size(), GENESIS_FIRST_CHAPTER_VERSES_COUNT);

  EXPECT_EQ(verses[0].verse, 1) << "Invalid first verse number";
  EXPECT_EQ(verses[0].text, first_verse_text) << "Invalid Gen 1:1 text";
  EXPECT_EQ(verses[verses.size() - 1].verse, 31) << "Invalid last verse number";
}

TEST_F(BibleToolboxTest, ExtractsClearedVersesWithoutStrongsTags) {
  constexpr auto GENESIS_BOOK_NUMBER = 10;
  constexpr auto second_verse_text =
      "And the earth was without form, and void; and darkness was upon the face of the deep. And the Spirit of God "
      "moved upon the face of the waters.";

  const auto excluded_strong = bible->versesInChapter(GENESIS_BOOK_NUMBER, 1, true);
  ASSERT_GE(excluded_strong.size(), 2);

  EXPECT_EQ(excluded_strong[0].text, "In the beginning God created the heaven and the earth.")
      << "Invalid Gen 1:1 cleared text";
  EXPECT_EQ(excluded_strong[1].text, second_verse_text) << "Invalid Gen 1:2 verse text";
}

TEST_F(BibleToolboxTest, ExtractsFamousVersesCorrectly) {
  constexpr auto john316_text =
      "<i>For<S>1063</S> God<S>2316</S> so<S>3779</S> loved<S>25</S> the world<S>2889</S>, that<S>5620</S> he "
      "gave<S>1325</S> his<S>846</S> only begotten<S>3439</S> Son<S>5207</S>, that<S>2443</S> whosoever<S>3956</S> "
      "believeth<S>4100</S> in<S>1519</S> him<S>846</S> should<S>622</S> not<S>3361</S> perish<S>622</S>, "
      "but<S>235</S> have<S>2192</S> everlasting<S>166</S> life<S>2222</S>.</i>";

  const auto john_verses = bible->versesInChapter(500, 3, false);
  ASSERT_GT(john_verses.size(), 15);
  EXPECT_EQ(john_verses[15].text, john316_text) << "Invalid John 3:16 text";
}

TEST_F(BibleToolboxTest, ReadingPlanInfoCorrect) {
  ASSERT_EQ("OY-p.plan", std::string(plan->id()));
  ASSERT_EQ(plan->daysCount(), 365);

  auto items = plan->locationsByDay(1);
  ASSERT_EQ(items.size(), 4);

  ASSERT_EQ(items[0].book, GENESIS_BOOK_NUMBER);
  ASSERT_EQ(items[0].range.startChapter, 1);
  ASSERT_EQ(items[0].range.startVerse, 1);
  ASSERT_EQ(items[0].range.endChapter, 2);
  ASSERT_EQ(items[0].range.endVerse, 25);
}

TEST_F(BibleToolboxTest, SingleChapterRangeReturnsOnlyBoundedVerses) {
  // Test case: Genesis 1:12 to 1:15 (within the same chapter)
  // Ensures the multi-chapter OR logic doesn't leak out all other verses in Ch 1.
  Location loc{.book = GENESIS_BOOK_NUMBER,
               .range = {.startChapter = 1, .startVerse = 12, .endChapter = 1, .endVerse = 15}};

  auto result = bible->versesByLocation(loc, true);

  ASSERT_EQ(result.size(), 4);
  EXPECT_EQ(result.front().chapter, 1);
  EXPECT_EQ(result.front().verse, 12);
  EXPECT_EQ(result.back().chapter, 1);
  EXPECT_EQ(result.back().verse, 15);
}

TEST_F(BibleToolboxTest, AdjacentChaptersRangeEvaluatesCorrectly) {
  // Test case: Genesis 1:30 to 2:2
  // Ensures the "middle chapters" logic (1 < chapter < 2) safely finds nothing
  // without choking or failing the query bounds.
  Location loc{.book = GENESIS_BOOK_NUMBER,
               .range = {.startChapter = 1, .startVerse = 30, .endChapter = 2, .endVerse = 2}};

  auto result = bible->versesByLocation(loc, true);

  // Assuming Gen 1 has 31 verses: grabs 1:30, 1:31, 2:1, 2:2
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(result.front().chapter, 1);
  EXPECT_EQ(result.front().verse, 30);
  EXPECT_EQ(result.back().chapter, 2);
  EXPECT_EQ(result.back().verse, 2);
}

TEST_F(BibleToolboxTest, MultiChapterRangeIncludesIntermediateChapters) {
  // Test case: Genesis 1:31 to 3:2
  // Ensures Chapter 2 is completely captured inside the range.
  Location loc{.book = GENESIS_BOOK_NUMBER,
               .range = {.startChapter = 1, .startVerse = 31, .endChapter = 3, .endVerse = 2}};

  auto result = bible->versesByLocation(loc, true);

  bool found_chapter_2 = false;
  for (const auto& verse : result) {
    if (verse.chapter == 2) {
      found_chapter_2 = true;
      break;
    }
  }
  EXPECT_TRUE(found_chapter_2) << "Middle chapter (Chapter 2) was completely skipped!";
}

TEST_F(BibleToolboxTest, InvalidInverseRangeReturnsEmptyVector) {
  // Test case: Start location is mathematically AFTER the end location (Gen 2:5 to 1:1)
  Location loc{.book = GENESIS_BOOK_NUMBER,
               .range = {.startChapter = 2, .startVerse = 5, .endChapter = 1, .endVerse = 1}};

  auto result = bible->versesByLocation(loc, true);

  ASSERT_EQ(result.size(), 0);
  EXPECT_TRUE(result.empty()) << "Query returned data for a structurally inverted range.";
}

TEST_F(BibleToolboxTest, ImpossibleHighVerseNumbersReturnEmptyVector) {
  // Test case: Querying a verse number that doesn't exist (e.g., Gen 1:200 to 1:255)
  Location loc{.book = GENESIS_BOOK_NUMBER,
               .range = {.startChapter = 1, .startVerse = 200, .endChapter = 1, .endVerse = 255}};

  auto result = bible->versesByLocation(loc, true);

  EXPECT_TRUE(result.empty());
}
