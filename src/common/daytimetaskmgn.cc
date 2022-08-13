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

#include "daytimetaskmgn.h"

static const int sDaySecs = 24*3600;

void CDayTimeTaskMgn::reinittime()
{
    time_t n = time(nullptr);
    struct tm t ;
    localtime_r(&n, &t);
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0 ;

    time_t mn = mktime(&t);
    m_begin  = mn;
    m_end = m_begin + sDaySecs;

    t.tm_hour = m_starthour;
    t.tm_min = m_startmin;
    t.tm_sec = m_startsec ;
    m_startTaskTime = mktime(&t);

    m_nextTaskTime = m_startTaskTime;
    while( m_nextTaskTime <= n ) {
        m_nextTaskTime += m_intervalsec;
    }

    return ;
}

bool CDayTimeTaskMgn::init(int starthour,int startmin,int startsec,int intervalsec)
{
    m_starthour = starthour;
    m_startmin = startmin;

    if( m_starthour == 0 && m_startmin == 0 ) {
        m_startmin = 1;
    }

    m_startsec = startsec;
    m_intervalsec = intervalsec;

    reinittime();
    return true;
}

bool CDayTimeTaskMgn::check(time_t t)
{
    if ( t < m_begin || t > m_end ) {
        reinittime();
    }

    if ( t >= m_nextTaskTime ) {
        do {
            m_nextTaskTime += m_intervalsec;
        }while(m_nextTaskTime <= t);
        
        return true;
    }
    
    return false;
}

