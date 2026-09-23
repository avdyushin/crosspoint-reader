#include "ReadingPlan.h"

#include "Location.h"
#include "Verse.h"

namespace BibleToolbox {
ReadingPlan::ReadingPlan(const std::filesystem::path& path, const char* vfs) {
  if (connection_.open(path, vfs)) {
    constexpr auto item = R"SQL(
        SELECT book_number, start_chapter, start_verse, end_chapter, end_verse
        FROM reading_plan
        WHERE day = ?;
    )SQL";
    if (statement_.prepare(connection_.get(), item)) {
      info_ = fetchInfo(path);
    }
  }
}

ReadingPlanInfo ReadingPlan::fetchInfo(const std::filesystem::path& path) const {
  const auto id = path.stem();
  constexpr auto sql = "SELECT name, value FROM info";
  auto statement = Statement();
  statement.prepare(connection_.get(), sql);
  std::string description;
  while (statement.step() != SQLITE_DONE) {
    const auto name = statement.getStringView(0);
    const auto value = statement.getString(1);
    if (name == "description") {
      description = value;
      break;
    }
  }
  return {
      .id = std::string(id),
      .description = description,
      .path = path,
  };
}

std::vector<Location> ReadingPlan::locationsByDay(const int day) const {
  std::vector<Location> references;
  statement_.reset();
  auto _ = statement_.bind(1, day);
  while (statement_.step() == SQLITE_ROW) {
    const auto reference = Location{
        .book = static_cast<bookNumber>(statement_.getInt(0)),
        .range =
            VersesRange{
                .startChapter = static_cast<chapterNumber>(statement_.getInt(1)),
                .startVerse = static_cast<verseNumber>(statement_.getInt(2)),
                .endChapter = static_cast<chapterNumber>(statement_.getInt(3)),
                .endVerse = static_cast<verseNumber>(statement_.getInt(4)),
            },
    };
    references.push_back(reference);
  }
  return references;
}
}  // namespace BibleToolbox
