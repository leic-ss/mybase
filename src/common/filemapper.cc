/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#include "filemapper.h"

namespace mybase
{

void FileMapper::closeFile()
{
	if (!mdata) {
		_log_warn(myLog, "no need to close file!");
		return ;
	}

    munmap(mdata, msize);
    close(fd);
    mdata = nullptr;
    msize = 0;
    fd = -1;
    return ;
}

void FileMapper::syncFile()
{
	if (!mdata || msize <= 0) {
		_log_err(myLog, "unable to sync file!");
		return ;
	}

    msync(mdata, msize, MS_ASYNC);
    return ;
}

bool FileMapper::openFile(const char* file_name, int32_t create_length)
{
    int32_t flags = PROT_READ;
    if (create_length > 0) {
        fd = open(file_name, O_RDWR | O_LARGEFILE | O_CREAT, 0644);
        flags = PROT_READ | PROT_WRITE;
    } else {
        fd = open(file_name, O_RDONLY | O_LARGEFILE);
    }

    if (fd < 0) {
        _log_err(myLog, "open file : %s failed, errno: %d", file_name, errno);
        return false;
    }

    if (create_length > 0) {
        if (ftruncate(fd, create_length) != 0) {
            _log_err(myLog, "ftruncate file: %s failed", file_name);
            close(fd);
            fd = -1;
            return false;
        }
        msize = create_length;
    } else {
        struct stat stbuff;
        fstat(fd, &stbuff);
        msize = stbuff.st_size;
    }

    mdata = (char*)mmap(0, msize, flags, MAP_SHARED, fd, 0);
    if (mdata == MAP_FAILED) {
    	_log_err(myLog, "map file: %s failed, err is %s(%d)", file_name, strerror(errno), errno);
        close(fd);
        fd = -1;
        mdata = nullptr;
        return false;
    }

    fileName = file_name;
    _log_info(myLog, "mmap file[%s] success! fd[%d]", file_name, fd);
    return true;
}

uint32_t FileMapper::getModifyTime() const
{
    struct stat buffer;
    if (fd >= 0 && fstat(fd, &buffer) == 0) {
        return (uint32_t)buffer.st_mtime;
    }
    return 0;
}

}
