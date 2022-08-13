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

#include "public/common.h"

#include <atomic>
#include <memory>

template <class ValueType>
class CPtrMgr {
public:
	using ValuePtr = std::shared_ptr<ValueType>;

public:
    CPtrMgr() {
    	index.store(0);
	    values[0] = ValuePtr();
	    values[1] = ValuePtr();
    }

    ~CPtrMgr() {
    }

    bool ExchangeValues(ValuePtr ptr) {

    	assert(nullptr != ptr.get());

    	locker.lock();
	    uint8_t idle = index.load() ^ 1;
	    if (0 != idle && 1 != idle) {
	    	locker.unlock();
	        return false;
	    }

	    values[idle] = ptr;
	    index.store(idle);
	    locker.unlock();

	    return true;
    }

    bool ExchangeIdleAndCurr() {

    	locker.lock();
    	uint8_t idle = index.load() ^ 1;
	    if (0 != idle && 1 != idle) {
	    	locker.unlock();
	        return false;
	    }

	    if (nullptr == values[idle].get()) {
	    	locker.unlock();
	        return false;
	    }
	    index.store(idle);
	    locker.unlock();

	    return true;
    }

    ValuePtr Curr() {
    	return values[index.load()];
    }

    ValuePtr NewIdle() {

    	locker.lock();
    	ValuePtr new_idle(new ValueType());
	    if (nullptr == new_idle.get()) {
	    	locker.unlock();
	        return ValuePtr();
	    }

	    uint8_t idle = index.load() ^ 1;
	    values[idle] = new_idle;
	    locker.unlock();

	    return new_idle;
    }

    ValuePtr Idle() {
    	uint8_t idle = index.load() ^ 1;
    	return values[idle];
    }

private:
    std::atomic<uint8_t> index;
    ValuePtr values[2];
    SpinLock locker;
};