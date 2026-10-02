#!/usr/bin/env bash
set -u

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 DIRECTORY" >&2
    exit 2
fi

directory=$1

if [[ ! -d "$directory" ]]; then
    echo "Error: not a directory: $directory" >&2
    exit 1
fi

page_size=1024

find "$directory" -type f \( -iname '*.sqlite3' \) -print0 |
while IFS= read -r -d '' database; do
    echo "Processing: $database"

    current_page_size=$(
        sqlite3 "$database" 'PRAGMA page_size;' 2>/dev/null
    ) || {
        echo "  ERROR: cannot open database"
        continue
    }

    if [[ "$current_page_size" == "$page_size" ]]; then
        echo "  Already using page size $page_size"
        continue
    fi

    # Optional safety backup
    cp -- "$database" "$database.bak" || {
        echo "  ERROR: backup failed"
        continue
    }

    if sqlite3 "$database" <<SQL
.timeout 5000
PRAGMA wal_checkpoint(TRUNCATE);
PRAGMA journal_mode = DELETE;
PRAGMA page_size = $page_size;
VACUUM;
SQL
    then
        new_page_size=$(
            sqlite3 "$database" 'PRAGMA page_size;' 2>/dev/null
        )

        if [[ "$new_page_size" == "$page_size" ]]; then
            echo "  Updated to page size $page_size"
        else
            echo "  ERROR: page size is $new_page_size"
        fi
    else
        echo "  ERROR: SQLite operation failed"
    fi
done
