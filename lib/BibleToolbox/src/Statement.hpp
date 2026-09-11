#pragma once
#include <sqlite3.h>

#include <string>

namespace BibleToolbox {
class Statement {
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement{nullptr, sqlite3_finalize};

 public:
  void prepare(sqlite3* db, const std::string_view sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.data(), -1, &stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db));
    }
    statement.reset(stmt);
  }

  [[nodiscard]] int step() const { return sqlite3_step(statement.get()); }

  void reset() const {
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
  }

  template <typename T>
  void bind(const int column, const T& value) const {
    if constexpr (std::is_integral_v<T> && sizeof(T) <= 4) {
      if (sqlite3_bind_int(statement.get(), column, value) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(statement.get())));
      }
    } else {
      throw std::runtime_error("Unsupported bind type");
    }
  }

  [[nodiscard]] std::string_view get_string_view(const int column) const {
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), column));
    if (text == nullptr) {
      return {};
    }

    const int bytes = sqlite3_column_bytes(statement.get(), column);
    return {text, static_cast<size_t>(bytes)};
  }

  [[nodiscard]] std::string get_string(const int column) const { return std::string{get_string_view(column)}; }

  [[nodiscard]] int get_int(const int column) const { return sqlite3_column_int(statement.get(), column); }
};
}  // namespace BibleToolbox
