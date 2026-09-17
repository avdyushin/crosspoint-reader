#pragma once
#include <BibleToolbox.h>

#include <format>

class BibleVerseFormatter {
 public:
  template <typename Output, BibleToolbox::BibleVersesProvider T>
    requires std::output_iterator<Output, const char&>
  void formatChapter(Output output, const T& provider, BibleToolbox::bookNumber book,
                     BibleToolbox::chapterNumber chapter, std::string_view chapterString) const {
    const auto verses = provider.chapterVerses(book, chapter, true);
    std::format_to(output, "<html><body>\n");
    if (chapter == 1) {
      std::format_to(output, "<h1>{}</h1>\n", provider[book]->name);
    }
    std::format_to(output, "<h2>{} {}</h2>\n", chapterString, chapter);
    for (const auto& v : verses) {
      std::format_to(output, "<p><sup>{} </sup> {}</p>\n", v.verse, v.text);
    }
    std::format_to(output, "</body></html>\n");
  }
};
