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

#include "defs.h"

#include <vector>

class CValidBucketMgn
{
public: 
    CValidBucketMgn() : m_fromMaster(false)
    {
        clean();
    }

    void clean()
    {
        bzero(m_bucketArray, sizeof(m_bucketArray));
        m_fromMaster = false;
    }

    void add(const std::vector<int32_t>& vec)
    {
        for( auto iter = vec.begin(); iter != vec.end(); ++iter ) {
            m_bucketArray[*iter] = 1;
        }
    }

    void setFromMaster(bool fromMaster) { m_fromMaster = fromMaster; }
    bool getFromMaster() const { return m_fromMaster; }

    inline const bool valid(int32_t bucket) const
    {
        if( !m_fromMaster ) { return true; }
        return m_bucketArray[bucket] == 1;
    }

private:
    char m_bucketArray[sMaxBucketNumber];
    bool m_fromMaster;
};