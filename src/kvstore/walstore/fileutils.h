#ifndef __COMMON_FS_FILEUTILS_H__
#define __COMMON_FS_FILEUTILS_H__

#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <vector>
#include <regex>
#include <iostream>
#include <fstream>

namespace walstore {

enum class FileType {
  UNKNOWN = 0,
  NOTEXIST,
  REGULAR,
  DIRECTORY,
  SYM_LINK,
  CHAR_DEV,
  BLOCK_DEV,
  FIFO,
  SOCKET
};

class FileUtils final {
public:
    FileUtils() = delete;

    static std::string dirname(const char* path);
    static std::string basename(const char* path);
    static std::string readLink(const char* path);
    static std::string realPath(const char* path);

    static size_t fileSize(const char* path);
    static FileType fileType(const char* path);
    static const char* getFileTypeName(FileType type);
    static time_t fileLastUpdateTime(const char* path);

    static bool isStdinTTY();
    static bool isStdoutTTY();
    static bool isStderrTTY();
    static bool isFdTTY(int fd);

    static std::string joinPath(const std::string& dir, const std::string& filename);
    static void dividePath(const std::string& path,
                           std::string& parent,
                           std::string& child);

    static bool remove(const char* path, bool recursively = false);
    static bool makeDir(const std::string& dir, uint32_t mode = 0775);
    static bool exist(const std::string& path);
    static bool rename(const std::string& src, const std::string& dst);

    static std::vector<std::string> listAllTypedEntitiesInDir(const char* dirpath,
                                                              FileType type,
                                                              bool returnFullPath,
                                                              const char* namePattern);
    static std::vector<std::string> listAllFilesInDir(const char* dirpath,
                                                      bool returnFullPath = false,
                                                      const char* namePattern = nullptr);
    static std::vector<std::string> listAllDirsInDir(const char* dirpath,
                                                     bool returnFullPath = false,
                                                     const char* namePattern = nullptr);

    static bool isReg(struct dirent* dEnt, const char* path);
    static bool isDir(struct dirent* dEnt, const char* path);
    static bool isLink(struct dirent* dEnt, const char* path);
    static bool isChr(struct dirent* dEnt, const char* path);
    static bool isBlk(struct dirent* dEnt, const char* path);
    static bool isFifo(struct dirent* dEnt, const char* path);
    static bool isSock(struct dirent* dEnt, const char* path);

    static uint64_t free(const char* path);

    static uint64_t available(const char* path);

    class Iterator;
    using DirEntryIterator = Iterator;
    using FileLineIterator = Iterator;
    class Iterator final {
    public:
        explicit Iterator(std::string path, const std::regex* pattern = nullptr);
        ~Iterator();

        bool valid() const { return status_; }
        void next();
        Iterator& operator++() {
          next();
          return *this;
        }
        Iterator operator++(int) = delete;

        std::string& entry() {
          return entry_;
        }

        const std::string& entry() const {
          return entry_;
        }

        std::smatch& matched() {
          return matched_;
        }

        const std::smatch& matched() const {
          return matched_;
        }

    private:
        void openFileOrDirectory();
        void dirNext();
        void fileNext();

    private:
        std::string path_;
        FileType type_{FileType::UNKNOWN};
        std::unique_ptr<std::ifstream> fstream_;
        DIR* dir_{nullptr};
        const std::regex* pattern_{nullptr};
        std::string entry_;
        std::smatch matched_;
        bool status_{true};
  };
};

}

#define CHECK_FILE_TYPE(NAME, FTYPE, DTYPE)                                               \
  bool FileUtils::is##NAME(struct dirent* dEnt, const char* path) {                  \
    if (dEnt->d_type == DT_UNKNOWN) {                                                \
      return FileUtils::fileType(FileUtils::joinPath(path, dEnt->d_name).c_str()) == \
             FileType::FTYPE;                                                        \
    } else {                                                                         \
      return dEnt->d_type == DT_##DTYPE;                                             \
    }                                                                                \
  }

#endif  // COMMON_FS_FILEUTILS_H_
