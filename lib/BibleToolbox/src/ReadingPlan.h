#pragma once

#include "Connection.h"
#include "Location.h"
#include "ReadingPlanInfo.h"
#include "Statement.h"

namespace BibleToolbox {
class ReadingPlan {
  Connection connection_ = Connection();
  Statement statement_ = Statement();
  ReadingPlanInfo info_;

  [[nodiscard]] ReadingPlanInfo fetchInfo(const std::filesystem::path& path) const;

 public:
  explicit ReadingPlan(const std::filesystem::path& path, const char* vfs);
  ~ReadingPlan() = default;

  [[nodiscard]] std::string_view id() const { return info_.id; }

  [[nodiscard]] std::vector<Location> locationsByDay(int day) const;
};
}  // namespace BibleToolbox
