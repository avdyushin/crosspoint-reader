# Holy Bible

## Schemas

Minimum expected schema:

```sqlite
CREATE TABLE verses (
        book_number NUMERIC NOT NULL, 
        chapter NUMERIC NOT NULL, 
        verse NUMERIC NOT NULL, 
        text TEXT NOT NULL DEFAULT '', 
        PRIMARY KEY (book_number, chapter, verse) );

CREATE TABLE info (
        name TEXT NOT NULL, 
        value TEXT NOT NULL, 
        PRIMARY KEY (name));

CREATE TABLE books (
        book_number NUMERIC NOT NULL, 
        short_name TEXT NOT NULL, 
        long_name TEXT NOT NULL, 
        book_color TEXT NOT NULL, 
        total_chapters INTEGER, 
        PRIMARY KEY (book_number));

CREATE INDEX idx_book_chapter ON verses (book_number, chapter);
```

### Info keys expected to set

* chapter_string (like 'Chapter') used as chapter prefix
* description (like 'King James Version of 1611/1769') used as book title
* language (like 'en') used for hyphenation

## Modules

https://www.ph4.org/b4_index.php

### Preconfigure module database

For databases without `total_chapters` column `books` table should be updated first.

Add total chapters column to book table:

```sqlite
ALTER TABLE books ADD COLUMN total_chapters INTEGER;

UPDATE books SET total_chapters = (
    SELECT COUNT(DISTINCT chapter) 
    FROM verses 
    WHERE verses.book_number = books.book_number
);

VACUUM;
```

Optimize for ESP32 low RAM, use 1024 page size:

```sqlite
PRAGMA page_size = 1024;
ANALYZE;
VACUUM;
```

Create index:

```sqlite
CREATE INDEX IF NOT EXISTS idx_book_chapter ON verses (book_number, chapter);
ANALYZE;
VACUUM;
```
