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

#pragma once

#include "public/dlog.h"

#include <vector>

#include <unistd.h>
#include <sys/mman.h>

namespace mybase
{

class FileMapper
{
public:
    FileMapper() { }
    ~FileMapper() { closeFile(); }

    void closeFile();

    void syncFile();

    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }

    // createLength == 0 means read only
    bool openFile(const char* file_name, int32_t create_length = 0);

    void *mData() const { return mdata; }

    int32_t getSize() const { return msize; }

    uint32_t getModifyTime() const;

private:
    FileMapper(const FileMapper&);
    FileMapper& operator = (const FileMapper&);

    mybase::BaseLogger* myLog{nullptr};
    std::string fileName;
    char* mdata{nullptr};
    int64_t msize{0};
    int32_t fd{-1};
};

}
