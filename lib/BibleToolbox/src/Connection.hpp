#pragma once
#include <sqlite3.h>

#include <filesystem>

namespace BibleToolbox {
template <typename F>
concept Transform = requires(F f, std::string_view s) {
  { f(s) } -> std::same_as<std::string>;
};

class Connection {
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection{nullptr, sqlite3_close};

 public:
  void open(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    if (sqlite3_initialize() != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db));
    }
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db));
    }
    connection.reset(db);
  }

  [[nodiscard]] sqlite3* get() const { return connection.get(); }
};
}  // namespace BibleToolbox
