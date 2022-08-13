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

#include <vector>
#include <mutex>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

class CBitMap
{
public:
	CBitMap(uint32_t num=31, bool safe=true) : isSafe(safe)
	{
		if (bitValue) {
			free(bitValue);
			bitValue = nullptr;
		}

		count = (num >> 3) + 1;
		bitValue = (uint8_t*)malloc(count);
		memset(bitValue, 0, count);

		bitSize = 0;
	}

	void resize(uint32_t num, bool flag=false)
	{
		if (isSafe) {
			std::lock_guard<std::mutex> l(mtx);
			resize_unsafe(num, flag);
		} else {
			resize_unsafe(num, flag);
		}
	}

	void reset()
	{
		if (isSafe) {
			std::lock_guard<std::mutex> l(mtx);
			reset_unsafe();
		} else {
			reset_unsafe();
		}
	}

	void set(uint32_t num)
	{
		if (isSafe) {
			std::lock_guard<std::mutex> l(mtx);
			set_unsafe(num);
		} else {
			set_unsafe(num);
		}
	}

	void clear(uint32_t num)
	{
		if (isSafe) {
			std::lock_guard<std::mutex> l(mtx);
			clear_unsafe(num);
		} else {
			clear_unsafe(num);
		}
	}

	bool test(uint32_t num)
	{
		if (isSafe) {
			std::lock_guard<std::mutex> l(mtx);
			return test_unsafe(num);
		} else {
			return test_unsafe(num);
		}
	}

	void resize_unsafe(uint32_t num, bool flag=false)
	{
		if (bitValue) {
			free(bitValue);
			bitValue = nullptr;
		}

		count = (num >> 3) + 1;
		bitValue = (uint8_t*)malloc(count);

		if (!flag) {
			memset(bitValue, 0, count);
			bitSize = 0;
		} else {
			memset(bitValue, 0xFF, count);
			bitSize = num;
		}
	}

	void reset_unsafe()
	{
		if (!bitValue) return ;
		memset(bitValue, 0, count);
	}

	void set_unsafe(uint32_t num) //set 1
	{
		if (!bitValue) return ;

		uint32_t index = num >> 3;
		uint32_t pos = num % 8;

		if ( ((bitValue[index] >> pos) & 0x01) != 1 ) {
			bitSize++;
		}

		bitValue[index] |= (1 << pos);
	}

	uint32_t size() { return bitSize; }

	void clear_unsafe(uint32_t num) //set 0
	{
		if (!bitValue) return ;

		uint32_t index = num >> 3;
		uint32_t pos = num % 8;
		if ( ((bitValue[index] >> pos) & 0x01) != 0 ) {
			bitSize--;
		}

		bitValue[index] &= ~(1 << pos);
	}

	bool test_unsafe(uint32_t num)
	{
		if (!bitValue) return false;

		uint32_t index = num >> 3;
		uint32_t pos = num % 8;
		bool flag = false;
		if (bitValue[index] & (1 << pos)) {
			flag = true;
		}

		return flag;
	}

private:
	bool isSafe;
	std::mutex mtx;

	uint32_t bitSize{0};
	uint32_t count;
	uint8_t* bitValue{nullptr};
};
