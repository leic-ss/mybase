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

#include <time.h>
#include <stdint.h>

class CDayTimeTaskMgn
{
public:
    CDayTimeTaskMgn():m_begin(0),m_end(0)
    {}

    bool init(int32_t starthour, int32_t startmin, int32_t startsec, int32_t intervalsec);
    bool check(time_t t);

private:
    void reinittime();

    int32_t m_starthour;
    int32_t m_startmin;
    int32_t m_startsec;
    int32_t m_intervalsec;

    int32_t m_startindex;

    time_t m_begin;
    time_t m_end;

    time_t m_startTaskTime;
    time_t m_nextTaskTime;
};

