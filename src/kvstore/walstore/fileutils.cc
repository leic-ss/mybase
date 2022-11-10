#include "fileutils.h"

#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/statfs.h>

#include <string.h>

namespace walstore {

static const int32_t kMaxPathLen = 1024;

namespace detail {

bool removeDir(const char* path, bool recursively)
{
    DIR* dh = opendir(path);
    if (!dh) return false;

    bool succeeded = true;
    struct dirent* dEnt;
    errno = 0;
    while (succeeded && !!(dEnt = readdir(dh))) {
        if (!strcmp(dEnt->d_name, ".") || !strcmp(dEnt->d_name, "..")) {
            continue;
        }

        if (FileUtils::isDir(dEnt, path) && !recursively) {
            succeeded = false;
        } else {
          succeeded = FileUtils::remove(FileUtils::joinPath(path, dEnt->d_name).c_str(), recursively);
          if (!succeeded) {
              //
          } else {
              //
          }
        }
    }

    if (succeeded && errno) {
        succeeded = false;
    }

    if (closedir(dh)) {
        return false;
    }

    if (!succeeded) {
        return false;
    }

    if (rmdir(path)) {
        return false;
    }
    return true;
}

}  // namespace detail

std::string FileUtils::readLink(const char* path)
{
    char buffer[kMaxPathLen];
    auto len = ::readlink(path, buffer, kMaxPathLen);
    if (len == -1) {
        return std::string();
    }
    return std::string(buffer, len);
}

std::string FileUtils::realPath(const char* path)
{
    char* buffer = ::realpath(path, NULL);
    if (buffer == NULL) {
        return std::string();
    }
    std::string truePath(buffer);
    ::free(buffer);
    return truePath;
}

std::string FileUtils::dirname(const char* path) {
    if (::strcmp("/", path) == 0) {
        return "/";
    }
    static const std::regex pattern("(.*)/([^/]+)/?");
    std::cmatch result;
    if (std::regex_match(path, result, pattern)) {
        if (result[1].first == result[1].second) {
            return "/";
        }
        return result[1].str();
    }
    return ".";
}

std::string FileUtils::basename(const char* path)
{
    if (::strcmp("/", path) == 0) {
        return "";
    }
    static const std::regex pattern("(/*([^/]+/+)*)([^/]+)/?");
    std::cmatch result;
    std::regex_match(path, result, pattern);
    return result[3].str();
}

const char* FileUtils::getFileTypeName(FileType type)
{
    static const char* kTypeNames[] = {"Unknown",
                                       "NotExist",
                                       "Regular",
                                       "Directory",
                                       "SoftLink",
                                       "CharDevice",
                                       "BlockDevice",
                                       "FIFO",
                                       "Socket"};

    return kTypeNames[static_cast<int>(type)];
}

size_t FileUtils::fileSize(const char* path)
{
    struct stat st;
    if (lstat(path, &st)) {
        return 0;
    }

    return st.st_size;
}

FileType FileUtils::fileType(const char* path)
{
    struct stat st;
    if (lstat(path, &st)) {
        if (errno == ENOENT) {
            return FileType::NOTEXIST;
        } else {
            return FileType::UNKNOWN;
        }
    }

    if (S_ISREG(st.st_mode)) {
        return FileType::REGULAR;
    } else if (S_ISDIR(st.st_mode)) {
        return FileType::DIRECTORY;
    } else if (S_ISLNK(st.st_mode)) {
        return FileType::SYM_LINK;
    } else if (S_ISCHR(st.st_mode)) {
        return FileType::CHAR_DEV;
    } else if (S_ISBLK(st.st_mode)) {
        return FileType::BLOCK_DEV;
    } else if (S_ISFIFO(st.st_mode)) {
        return FileType::FIFO;
    } else if (S_ISSOCK(st.st_mode)) {
        return FileType::SOCKET;
    }

    return FileType::UNKNOWN;
}

int64_t FileUtils::fileLastUpdateTime(const char* path)
{
    struct stat st;
    if (lstat(path, &st)) {
        return -1;
    }
    return st.st_mtime;
}

bool FileUtils::isStdinTTY() { return isFdTTY(::fileno(stdin)); }

bool FileUtils::isStdoutTTY() { return isFdTTY(::fileno(stdout)); }

bool FileUtils::isStderrTTY() { return isFdTTY(::fileno(stderr)); }

bool FileUtils::isFdTTY(int fd) { return ::isatty(fd) == 1; }

std::string FileUtils::joinPath(const std::string& dir, const std::string& filename)
{
    std::string buf;

    std::size_t len = dir.size();
    if (len == 0) {
        buf.resize(filename.size() + 2);
        strcpy(&(buf[0]), "./");
        strncpy(&(buf[2]), filename.data(), filename.size());
        return buf;
    }

    if (dir[len - 1] == '/') {
        buf.resize(len + filename.size());
        strncpy(&(buf[0]), dir.data(), len);
    } else {
        buf.resize(len + filename.size() + 1);
        strncpy(&(buf[0]), dir.data(), len);
        buf[len++] = '/';
    }

    strncpy(&(buf[len]), filename.data(), filename.size());
    return buf;
}

void FileUtils::dividePath(const std::string& path,
                           std::string& parent,
                           std::string& child)
{
    if (path.empty() || path == "/") {
        parent = std::string();
        child = path;
        return;
    }

    std::string pathToLook = (path.back() == '/') ? std::string(path.data(), path.size() - 1) : path;
    auto pos = pathToLook.rfind('/');
    if (pos == std::string::npos) {
        parent = std::string();
        child = pathToLook;
        return;
    }

    child = std::string(pathToLook.data() + pos + 1, pathToLook.size() - pos - 1);
    if (pos == 0) {
        parent = std::string(pathToLook.data(), 1);
    } else {
        parent = std::string(pathToLook.data(), pos);
    }
}

bool FileUtils::remove(const char* path, bool recursively)
{
    auto type = fileType(path);
    switch (type) {
        case FileType::REGULAR:
        case FileType::SYM_LINK:
            if (unlink(path)) {
                return false;
            }
            return true;
        case FileType::DIRECTORY:
            return detail::removeDir(path, recursively);
        case FileType::CHAR_DEV:
        case FileType::BLOCK_DEV:
        case FileType::FIFO:
        case FileType::SOCKET:
            return false;
        case FileType::NOTEXIST:
            return true;
        default:
            return false;
    }
}

bool FileUtils::makeDir(const std::string& dir, uint32_t mode)
{
    if (dir.empty()) {
        return false;
    }
    FileType type = fileType(dir.c_str());
    if (type == FileType::DIRECTORY || type == FileType::SYM_LINK) {
        return true;
    } else if (type != FileType::NOTEXIST) {
        return false;
    }

    std::string parent;
    std::string child;
    dividePath(dir, parent, child);

    if (!parent.empty()) {
        bool ret = makeDir(parent.c_str(), mode);
        if (!ret) {
           return false;
        }
    }

    int err = mkdir(dir.c_str(), mode);
    if (err != 0) {
        return fileType(dir.c_str()) == FileType::DIRECTORY;
    }
    return true;
}

bool FileUtils::exist(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    return access(path.c_str(), F_OK) == 0;
}

bool FileUtils::rename(const std::string& src, const std::string& dst)
{
    auto status = ::rename(src.c_str(), dst.c_str());
    return status == 0;
}

uint64_t FileUtils::free(const char* path)
{
    struct statfs diskInfo;
    int err = statfs(path, &diskInfo);
    if (err != 0) {
        return (uint64_t)-1;
    }
    return diskInfo.f_bfree * diskInfo.f_bsize;
}

uint64_t FileUtils::available(const char* path)
{
    struct statfs diskInfo;
    int err = statfs(path, &diskInfo);
    if (err != 0) {
        return (uint64_t)-1;
    }
    return diskInfo.f_bavail * diskInfo.f_bsize;
}

std::vector<std::string> FileUtils::listAllTypedEntitiesInDir(const char* dirpath,
                                                              FileType type,
                                                              bool returnFullPath,
                                                              const char* namePattern)
{
    std::vector<std::string> entities;
    struct dirent* dirInfo;
    DIR* dir = opendir(dirpath);
    if (dir == nullptr) {
        return entities;
    }

    while ((dirInfo = readdir(dir)) != nullptr) {
        if ((type == FileType::REGULAR && FileUtils::isReg(dirInfo, dirpath)) ||
            (type == FileType::DIRECTORY && FileUtils::isDir(dirInfo, dirpath)) ||
            (type == FileType::SYM_LINK && FileUtils::isLink(dirInfo, dirpath)) ||
            (type == FileType::CHAR_DEV && FileUtils::isChr(dirInfo, dirpath)) ||
            (type == FileType::BLOCK_DEV && FileUtils::isBlk(dirInfo, dirpath)) ||
            (type == FileType::FIFO && FileUtils::isFifo(dirInfo, dirpath)) ||
            (type == FileType::SOCKET && FileUtils::isSock(dirInfo, dirpath))) {
          if (!strcmp(dirInfo->d_name, ".") || !strcmp(dirInfo->d_name, "..")) {
              continue;
          }
          if (namePattern && fnmatch(namePattern, dirInfo->d_name, FNM_FILE_NAME | FNM_PERIOD)) {
              continue;
          }

          entities.emplace_back(returnFullPath ? joinPath(dirpath, std::string(dirInfo->d_name))
                                               : std::string(dirInfo->d_name));
        }
    }
    closedir(dir);

    return entities;
}

std::vector<std::string> FileUtils::listAllFilesInDir(const char* dirpath,
                                                      bool returnFullPath,
                                                      const char* namePattern)
{
    return listAllTypedEntitiesInDir(dirpath, FileType::REGULAR, returnFullPath, namePattern);
}

std::vector<std::string> FileUtils::listAllDirsInDir(const char* dirpath,
                                                     bool returnFullPath,
                                                     const char* namePattern)
{
    return listAllTypedEntitiesInDir(dirpath, FileType::DIRECTORY, returnFullPath, namePattern);
}

FileUtils::Iterator::Iterator(std::string path, const std::regex* pattern) : path_(std::move(path))
{
    pattern_ = pattern;
    openFileOrDirectory();
    if (status_) {
        next();
    }
}

FileUtils::Iterator::~Iterator()
{
    if (fstream_ != nullptr && fstream_->is_open()) {
        fstream_->close();
    }
    if (dir_ != nullptr) {
        ::closedir(dir_);
      dir_ = nullptr;
    }
}

void FileUtils::Iterator::next()
{
    while (true) {
        if (type_ == FileType::DIRECTORY) {
            dirNext();
        } else {
            fileNext();
        }
        if (!status_) {
            return;
        }
        if (pattern_ != nullptr) {
            if (!std::regex_search(entry_, matched_, *pattern_)) {
                continue;
            }
        }
        break;
    }
}

void FileUtils::Iterator::dirNext()
{
    struct dirent* dent;
    while ((dent = ::readdir(dir_)) != nullptr) {
        if (dent->d_name[0] == '.') {
            continue;
        }
        break;
    }
    if (dent == nullptr) {
        status_ = false;
        return;
    }
    entry_ = dent->d_name;
}

void FileUtils::Iterator::fileNext()
{
    if (!std::getline(*fstream_, entry_)) {
        status_ = false;
    }
}

void FileUtils::Iterator::openFileOrDirectory()
{
    type_ = FileUtils::fileType(path_.c_str());
    if (type_ == FileType::DIRECTORY) {
        if ((dir_ = ::opendir(path_.c_str())) == nullptr) {
            status_ = false;
            return;
        }
    } else if (type_ == FileType::REGULAR) {
        std::unique_ptr<std::ifstream> fiter( new std::ifstream() );
        fstream_ = std::move(fiter);
        fstream_->open(path_);
        if (!fstream_->is_open()) {
            status_ = false;
            return;
        }
    } else if (type_ == FileType::SYM_LINK) {
        auto result = FileUtils::realPath(path_.c_str());
        if (result.empty()) {
            status_ = false;
            return;
        }
        path_ = std::move(result);
        openFileOrDirectory();
    } else {
        status_ = false;
        return;
    }
    status_ = true;
}

CHECK_FILE_TYPE(Reg, REGULAR, REG)
CHECK_FILE_TYPE(Dir, DIRECTORY, DIR)
CHECK_FILE_TYPE(Link, SYM_LINK, LNK)
CHECK_FILE_TYPE(Chr, CHAR_DEV, CHR)
CHECK_FILE_TYPE(Blk, BLOCK_DEV, BLK)
CHECK_FILE_TYPE(Fifo, FIFO, FIFO)
CHECK_FILE_TYPE(Sock, SOCKET, SOCK)

}
