#include "sqlite3_hal.h"

#include <HalStorage.h>
#include <Logging.h>
#include <sys/time.h>

#include <memory>

namespace {
constexpr auto LOG_ORIGIN = "SQLITE3";
constexpr int MAX_PATH_NAME = 128;

struct file_wrapper {
  sqlite3_file base;
  std::unique_ptr<HalFile> hal;
};

int access(sqlite3_vfs*, const char* zPath, int, int*) {
  if (!Storage.exists(zPath)) {
    return SQLITE_ACCESS_EXISTS;
  }
  return SQLITE_OK;
}

int full_pathname(sqlite3_vfs*, const char* zPath, const int nPathOut, char* zPathOut) {
  strncpy(zPathOut, zPath, nPathOut);
  zPathOut[nPathOut - 1] = '\0';
  return SQLITE_OK;
}

int read(sqlite3_file* pFile, void* zBuffer, int iAmount, sqlite_int64 iOffset) {
  const file_wrapper* file = reinterpret_cast<file_wrapper*>(pFile);
  if (!file->hal->seekSet(iOffset)) {
    return SQLITE_IOERR_SEEK;
  }
  const auto readSize = file->hal->read(zBuffer, iAmount);
  if (readSize != iAmount) {
    return SQLITE_IOERR_READ;
  }
  return SQLITE_OK;
}

int file_size(sqlite3_file* pFile, sqlite_int64* pSize) {
  const file_wrapper* file = reinterpret_cast<file_wrapper*>(pFile);
  const size_t size = file->hal->fileSize();
  *pSize = static_cast<sqlite3_int64>(size);
  return SQLITE_OK;
}

constexpr sqlite3_io_methods io_methods = {
    .iVersion = 3,
    .xClose = [](sqlite3_file*) { return SQLITE_OK; },
    .xRead = read,
    .xFileSize = file_size,
    .xLock = [](sqlite3_file*, int) { return SQLITE_OK; },
    .xUnlock = [](sqlite3_file*, int) { return SQLITE_OK; },
    .xCheckReservedLock =
        [](sqlite3_file*, int* pResOut) {
          *pResOut = 0;
          return SQLITE_OK;
        },
    .xFileControl = [](sqlite3_file*, int, void*) { return SQLITE_OK; },
    .xSectorSize = [](sqlite3_file*) { return 0; },
    .xDeviceCharacteristics = [](sqlite3_file*) { return SQLITE_IOCAP_IMMUTABLE; }};

int open(sqlite3_vfs*, const char* zName, sqlite3_file* pFile, const int flags, int* pOutFlags) {
  if (flags & SQLITE_OPEN_MAIN_DB) {
    LOG_INF(LOG_ORIGIN, "Open main database %s", zName);
  }
  if (flags & SQLITE_OPEN_MAIN_JOURNAL || flags & SQLITE_OPEN_MASTER_JOURNAL) {
    LOG_INF(LOG_ORIGIN, "Open journal %s is not implemented yet", zName);
    return SQLITE_CANTOPEN;
  }
  if (zName == nullptr) {
    LOG_INF(LOG_ORIGIN, "Open temp file is not implemented yet");
    return SQLITE_CANTOPEN;
  }
  auto* file = reinterpret_cast<file_wrapper*>(pFile);
  file->hal = std::make_unique<HalFile>();
  if (!Storage.openFileForRead(LOG_ORIGIN, zName, *file->hal)) {
    LOG_INF(LOG_ORIGIN, "Can open HAL file %s", zName);
    return SQLITE_CANTOPEN;
  }
  if (pOutFlags) {
    *pOutFlags = flags;
  }
  file->base.pMethods = &io_methods;
  LOG_INF(LOG_ORIGIN, "Successfully opened %s", zName);
  return SQLITE_OK;
}

sqlite3_vfs minimum_vfs = {
    .iVersion = 3,
    .szOsFile = sizeof(file_wrapper),
    .mxPathname = MAX_PATH_NAME,
    .pNext = nullptr,
    .zName = HAL_VFS_NAME,
    .pAppData = nullptr,
    .xOpen = open,
    .xAccess = access,
    .xFullPathname = full_pathname,
};
}  // namespace

extern "C" {
void sqlite3_log_callback(void*, const int iErrCode, const char* zMsg) {
  LOG_ERR(LOG_ORIGIN, "Error (%d): %s", iErrCode, zMsg);
}

int sqlite3_os_init() {
  sqlite3_config(SQLITE_CONFIG_LOG, sqlite3_log_callback, NULL);
  LOG_INF(LOG_ORIGIN, "os_init(): register VFS: %s", minimum_vfs.zName);
  return sqlite3_vfs_register(&minimum_vfs, true);
}

int sqlite3_os_end() {
  LOG_INF(LOG_ORIGIN, "os_end()");
  return SQLITE_OK;
}
}
