#pragma once
#include <sqlite3_hal.h>

#include <filesystem>

namespace BibleToolbox {
template <typename F>
concept Transform = requires(F f, std::string_view s) {
  { f(s) } -> std::same_as<std::string>;
};

class Connection {
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection{nullptr, sqlite3_close};

 public:
  bool open(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    if (sqlite3_initialize() != SQLITE_OK) {
      return false;
    }
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, HAL_VFS_NAME) != SQLITE_OK) {
      return false;
    }
    connection.reset(db);
    return true;
  }

  [[nodiscard]] sqlite3* get() const { return connection.get(); }

  [[nodiscard]] bool isOpen() const { return connection != nullptr; }
};
}  // namespace BibleToolbox
