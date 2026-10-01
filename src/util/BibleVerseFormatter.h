#pragma once

#include <DailyReadingProvider.h>
#include <VerseProvider.h>

#include <format>

class BibleVerseFormatter {
 public:
  template <typename Output, BibleToolbox::VersesProvider T>
    requires std::output_iterator<Output, const char&>
  void formatChapter(Output output, const T& provider, BibleToolbox::bookNumber book,
                     BibleToolbox::chapterNumber chapter, std::string_view chapterString) const {
    const auto verses = provider.versesInChapter(book, chapter, true);
    std::format_to(output, "<html><body>\n");
    if (chapter == 1) {
      std::format_to(output, "<h1>{}</h1>\n", provider[book]->name);
    }
    std::format_to(output, "<h2>{} {}</h2>\n", chapterString, chapter);
    for (const auto& verse : verses) {
      std::format_to(output, "<p><sup>{} </sup> {}</p>\n", verse.verse, verse.text);
    }
    std::format_to(output, "</body></html>\n");
  }

  template <typename Output, BibleToolbox::DailyReadingProvider Plan, BibleToolbox::VersesProvider Provider>
    requires std::output_iterator<Output, const char&>
  void readingDayVerses(Output output, const Plan& plan, const Provider& provider, const int day,
                        std::string_view dayString) const {
    const auto locations = plan.locationsByDay(day);
    std::format_to(output, "<html><body>\n");
    std::format_to(output, "<h1>{} {}</h1>\n", dayString, day);
    for (const auto& location : locations) {
      std::format_to(output, "<h2>{}</h2>\n", provider.locationToString(location));
      for (const auto verses = provider.versesByLocation(location, true); const auto& verse : verses) {
        std::format_to(output, "<p><sup>{}:{} </sup> {}</p>\n", verse.chapter, verse.verse, verse.text);
      }
    }
    std::format_to(output, "</body></html>\n");
  }
};
