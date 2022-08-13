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

#include <stdint.h>

namespace mybase {

class CAsyncLimit
{
public:
	CAsyncLimit(uint32_t max_ops_per_sec = 0, uint32_t smooth_timer=10);

	void limitrate();

	CAsyncLimit& operator=(const CAsyncLimit& from);

public:
	uint32_t maxOpsPerSec;
	float maxOpsPerMSec;

	uint32_t opsCurSec;
	uint64_t curTimerIndex;
	uint32_t smoothTimer;
};

}
