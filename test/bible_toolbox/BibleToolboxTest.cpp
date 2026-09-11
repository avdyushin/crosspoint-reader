#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "lib/BibleToolbox/src/BibleToolbox.h"

using namespace BibleToolbox;

namespace {
class MockVersesProvider {
  const Book book_ = Book{.name = "Genesis"};

 public:
  const Book* operator[](bookNumber) const { return &book_; }
  static std::vector<Verse> chapter_verses(bookNumber, chapterNumber, bool) {
    return std::vector{
        Verse{.verse = 1, .text = "First verse."},
        Verse{.verse = 2, .text = "Last verse."},
    };
  }
};
}  // namespace

class BibleToolboxTest : public ::testing::Test {
 protected:
  std::filesystem::path path;
  std::unique_ptr<Bible> bible;

  void SetUp() override {
    path = std::filesystem::path(__FILE__).parent_path() / "assets" / "KJV+.SQLite3";
    bible = std::make_unique<Bible>(path);
  }
};

TEST_F(BibleToolboxTest, ValidatesBookAndChapterCounts) {
  constexpr auto TOTAL_BOOKS = 66;
  constexpr auto TOTAL_CHAPTERS = 1189;

  ASSERT_EQ(bible->books().size(), TOTAL_BOOKS);

  const auto last_book = bible->books().back();
  EXPECT_EQ(last_book.prefixSum + last_book.chaptersCount, TOTAL_CHAPTERS) << "Invalid global number of chapters";
}

TEST_F(BibleToolboxTest, HandlesBookLookupsAndBounds) {
  constexpr auto GENESIS_BOOK_NUMBER = 10;
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

  const auto verses = bible->chapterVerses(GENESIS_BOOK_NUMBER, 1, false);
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

  const auto excluded_strong = bible->chapterVerses(GENESIS_BOOK_NUMBER, 1, true);
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

  const auto john_verses = bible->chapterVerses(500, 3, false);
  ASSERT_GT(john_verses.size(), 15);
  EXPECT_EQ(john_verses[15].text, john316_text) << "Invalid John 3:16 text";
}
