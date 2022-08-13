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

#include "asynclimit.h"

#include "common.h"

#include <unistd.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

namespace mybase {

CAsyncLimit::CAsyncLimit(uint32_t max_ops_per_sec, uint32_t smooth_timer)
						: maxOpsPerSec(max_ops_per_sec)
						, opsCurSec(0)
						, curTimerIndex(0)
						, smoothTimer(smooth_timer)
{
	assert(smooth_timer != 0);
	if( 0 == max_ops_per_sec ) {
		maxOpsPerSec = 100000;
	}
	if (maxOpsPerSec == 0) {
		maxOpsPerMSec = 1;
	} else {
		maxOpsPerMSec = ((float)maxOpsPerSec)/1000;
	}
}

CAsyncLimit& CAsyncLimit::operator=(const CAsyncLimit& from)
{
	this->maxOpsPerSec = from.maxOpsPerSec;
	this->maxOpsPerMSec = from.maxOpsPerMSec;
	this->curTimerIndex = from.curTimerIndex;
	this->opsCurSec = from.opsCurSec;
	return *this;
}

void CAsyncLimit::limitrate()
{
	uint64_t cur_msec = TimeHelper::currentMs();
	if( (cur_msec / smoothTimer) != curTimerIndex ) {
		// fprintf(stderr, "%lu %u %f %lu %lu\n", cur_msec, opsCurSec, maxOpsPerMSec, (cur_msec / smoothTimer), curTimerIndex);
		curTimerIndex = cur_msec / smoothTimer;
		opsCurSec = 1;
		return ;
	}

	opsCurSec++;
	uint32_t walk_msec = cur_msec % smoothTimer;
	uint32_t should_msec = (uint32_t)(opsCurSec/maxOpsPerMSec);
	// fprintf(stderr, "%lu %u %u %u %f %lu\n", cur_msec, walk_msec, should_msec, opsCurSec, maxOpsPerMSec, curTimerIndex);
	if( should_msec > walk_msec ) {
		// fprintf(stderr, "usleep %u\n", 1000 * (should_msec - walk_msec));
		usleep(1000 * (should_msec - walk_msec));
	}
}


}
