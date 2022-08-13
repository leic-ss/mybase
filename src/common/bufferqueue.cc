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

#include "bufferqueue.h"

using namespace std;

static const auto _buffer_deleter_ = [](char* ptr){ delete[] ptr; return ; };

namespace mybase  
{

std::atomic<uint64_t> CLockBufferQueue::m_queueTotalNum(0);
std::atomic<uint64_t> CLockBufferQueue::m_queueTotalMem(0);

void CBufferQueue::attach(char* pBuf, uint64_t iBufSize) noexcept(false)
{
	if((uint64_t)pBuf % sizeof(uint64_t) != 0) { // 保护
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(uint64_t) == 0");
	}
	if(iBufSize <= sizeof(Header) + sizeof(uint64_t)+ReserveLen) {
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(uint64_t) == 0");
	}

	_header = (Header *)pBuf;
	_data = pBuf+sizeof(Header);

	if(_header->iBufSize != iBufSize - sizeof(Header))
		throw runtime_error("CBufferQueue::attach fail: iBufSize != iBufSize - sizeof(Header);");
	if(_header->iReserveLen != ReserveLen)
		throw runtime_error("CBufferQueue::attach fail: iReserveLen != ReserveLen");
	if(_header->iBegin >= _header->iBufSize)
		throw runtime_error("CBufferQueue::attach fail: iBegin > iBufSize - sizeof(Header);");
	if(_header->iEnd > iBufSize - sizeof(Header))
		throw runtime_error("CBufferQueue::attach fail: iEnd > iBufSize - sizeof(Header);");
}

void CBufferQueue::create(char* pBuf, uint64_t iBufSize) noexcept(false)
{
	if((uint64_t)pBuf % sizeof(uint64_t) != 0) { // 保护
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(uint64_t) == 0");
	}
	if(iBufSize <= sizeof(Header)+sizeof(uint64_t)+ReserveLen) {
		throw runtime_error("CBufferQueue::create fail:pBuf must % sizeof(uint64_t) == 0");
	}

	_header = (Header *)pBuf;
	_data = pBuf+sizeof(Header);

	_header->iBufSize = iBufSize - sizeof(Header);
	_header->iReserveLen = ReserveLen;
	_header->iBegin = 0;
	_header->iEnd = 0;
	_header->iCount = 0; 
}

bool CBufferQueue::dequeue(char *buffer, uint32_t& buffersize) noexcept(false)
{
	if(isEmpty()) {
		return false;
	}

    if(_header->iCount) {
       _header->iCount--; 
    }
    
	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(uint64_t) < _header->iEnd);
		uint64_t len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(uint64_t)+len <= _header->iEnd);
		if(len > buffersize) {
			_header->iBegin += len+sizeof(uint64_t);
			throw buffer_full("CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		memcpy(buffer,_data+_header->iBegin+sizeof(uint64_t),len);
		_header->iBegin += len+sizeof(uint64_t);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		uint64_t len = 0;
		uint64_t new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(uint64_t) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(uint64_t)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(uint64_t)-_header->iBufSize); //
			new_begin = _header->iBegin+sizeof(uint64_t)-_header->iBufSize+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(uint64_t);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if( (_header->iBegin + sizeof(uint64_t) + len) < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(uint64_t)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin + sizeof(uint64_t) + len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			memcpy(buffer,data_from,len);
		} else {
			memcpy(buffer,data_from,_data-data_from+_header->iBufSize);
			memcpy(buffer+(_data-data_from+_header->iBufSize),_data,data_to-_data);
			assert(_header->iBufSize-(data_from-data_to)== len);
		}
	}

	return true;
}


bool CBufferQueue::peek(char *buffer, uint32_t& buffersize) noexcept(false)
{
	if(isEmpty()) {
		return false;
	}

	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(uint64_t) < _header->iEnd);
		uint64_t len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(uint64_t)+len <= _header->iEnd);
		if(len > buffersize) {
			throw buffer_full("CBufferQueue::peek data is too long to store in the buffer");
		}
		buffersize = len;
		memcpy(buffer,_data+_header->iBegin + sizeof(uint64_t), len);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		uint64_t len = 0;
		uint64_t new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(uint64_t) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(uint64_t)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(uint64_t)-_header->iBufSize); //
			new_begin = _header->iBegin+sizeof(uint64_t)-_header->iBufSize+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(uint64_t);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(uint64_t)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(uint64_t)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(uint64_t)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		//_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("mybase::CBufferQueue::peek data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			memcpy(buffer,data_from,len);
		} else {
			memcpy(buffer,data_from,_data-data_from+_header->iBufSize);
			memcpy(buffer+(_data-data_from+_header->iBufSize),_data,data_to-_data);
			assert(_header->iBufSize-(data_from-data_to)== len);
		}
	}

	return true;
}

bool CBufferQueue::dequeue(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) noexcept(false)
{
	uint64_t buffersize = buffersize1+buffersize2;

	if(isEmpty()) {
		return false;
	}

    if(_header->iCount)
    {
       _header->iCount--; 
    }
       
	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(uint64_t) < _header->iEnd);
		uint64_t len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(uint64_t)+len <= _header->iEnd);
		if(len > buffersize) {
			_header->iBegin += len+sizeof(uint64_t);
			cerr << "mybase::CBufferQueue::dequeue: len=" << len << " bufsize=" << buffersize <<",tid:%ld"<<pthread_self() << endl;
			throw buffer_full("mybase::CBufferQueue::dequeue data is too long tostore in the buffer");
		}
		if(buffersize1 > len) {
			buffersize1 = len;
			buffersize2 = 0;
			memcpy(buffer1,_data+_header->iBegin+sizeof(uint64_t),len);
		} else {
			buffersize2 = len-buffersize1;
			memcpy(buffer1,_data+_header->iBegin+sizeof(uint64_t),buffersize1);
			memcpy(buffer2,_data+_header->iBegin+sizeof(uint64_t)+buffersize1,buffersize2);
		}
		_header->iBegin += len+sizeof(uint64_t);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		uint64_t len = 0;
		uint64_t new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(uint64_t) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(uint64_t)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(uint64_t)-_header->iBufSize); //
			new_begin = (_header->iBegin+sizeof(uint64_t)-_header->iBufSize)+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(uint64_t);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(uint64_t)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(uint64_t)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(uint64_t)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("mybase::CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			if(buffersize1 > len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,len);
			} else {
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,buffersize2);
			}
		} else {
			assert(_header->iBufSize-(data_from-data_to)== len);
			if(buffersize1>len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,data_to-_data);
			} else if(buffersize1>_data-data_from+_header->iBufSize) { //buffer1被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,buffersize1-(_data+_header->iBufSize-data_from));
				memcpy(buffer2,data_from+buffersize1-_header->iBufSize,buffersize2);
			} else { //buffer2被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,_data+_header->iBufSize-data_from-buffersize1);
				memcpy(buffer2+(_data+_header->iBufSize-data_from-buffersize1),_data,len-(_data-data_from+_header->iBufSize));
			}
		}
	}

	return true;
}
bool CBufferQueue::peek(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2) noexcept(false)
{
	uint64_t buffersize = buffersize1+buffersize2;

	if(isEmpty()) {
		return false;
	}

	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(uint64_t) < _header->iEnd);
		uint64_t len = GetLen(_data+_header->iBegin);
		assert(_header->iBegin+sizeof(uint64_t)+len <= _header->iEnd);
		if(len > buffersize) {
			_header->iBegin += len+sizeof(uint64_t);
			// cerr << "mybase::CBufferQueue::dequeue: len=" << len << " bufsize=" << buffersize << endl;
			throw buffer_full("mybase::CBufferQueue::dequeue data is too long tostore in the buffer");
		}
		if(buffersize1 > len) {
			buffersize1 = len;
			buffersize2 = 0;
			memcpy(buffer1,_data+_header->iBegin+sizeof(uint64_t),len);
		} else {
			buffersize2 = len-buffersize1;
			memcpy(buffer1,_data+_header->iBegin+sizeof(uint64_t),buffersize1);
			memcpy(buffer2,_data+_header->iBegin+sizeof(uint64_t)+buffersize1,buffersize2);
		}
		//_header->iBegin += len+sizeof(uint64_t);
	} else {
		// 被分段
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
		uint64_t len = 0;
		uint64_t new_begin = 0;
		char *data_from = NULL;
		char *data_to = NULL;
		assert(_header->iBegin+1 <= _header->iBufSize);
		// 长度字段也被分段
		if(_header->iBegin+sizeof(uint64_t) > _header->iBufSize) { 
			char tmp[16];
			memcpy(tmp,_data+_header->iBegin,_header->iBufSize-_header->iBegin);
			memcpy(tmp+_header->iBufSize-_header->iBegin,_data,_header->iBegin+sizeof(uint64_t)-_header->iBufSize);
			len = GetLen(tmp);
			data_from = _data+(_header->iBegin+sizeof(uint64_t)-_header->iBufSize); //
			new_begin = (_header->iBegin+sizeof(uint64_t)-_header->iBufSize)+len;
			assert(new_begin <= _header->iEnd);
		} else {
			len = GetLen(_data+_header->iBegin);
			data_from = _data+_header->iBegin+sizeof(uint64_t);
			if(data_from == _data+_header->iBufSize) data_from = _data;
			if(_header->iBegin+sizeof(uint64_t)+len < _header->iBufSize) { 
				new_begin = _header->iBegin+sizeof(uint64_t)+len;
			} else { // 数据被分段
				new_begin = _header->iBegin+sizeof(uint64_t)+len-_header->iBufSize;
				assert(new_begin <= _header->iEnd);
			}
		}
		data_to = _data+new_begin;
		//_header->iBegin = new_begin;

		if(len > buffersize) {
			throw buffer_full("mybase::CBufferQueue::dequeue data is too long to store in the buffer");
		}
		buffersize = len;
		if(data_to > data_from) {
			assert(data_to - data_from == (long)len);
			if(buffersize1 > len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,len);
			} else {
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,buffersize2);
			}
		} else {
			assert(_header->iBufSize-(data_from-data_to)== len);
			if(buffersize1>len) {
				buffersize1 = len;
				buffersize2 = 0;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,data_to-_data);
			} else if(buffersize1>_data-data_from+_header->iBufSize) { //buffer1被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,_data+_header->iBufSize-data_from);
				memcpy(buffer1+(_data+_header->iBufSize-data_from),_data,buffersize1-(_data+_header->iBufSize-data_from));
				memcpy(buffer2,data_from+buffersize1-_header->iBufSize,buffersize2);
			} else { //buffer2被分段
				buffersize2 = len-buffersize1;
				memcpy(buffer1,data_from,buffersize1);
				memcpy(buffer2,data_from+buffersize1,_data+_header->iBufSize-data_from-buffersize1);
				memcpy(buffer2+(_data+_header->iBufSize-data_from-buffersize1),_data,len-(_data-data_from+_header->iBufSize));
			}
		}
	}

	return true;
}
void CBufferQueue::enqueue(const char *buffer,unsigned len) noexcept(false)
{
	if(len == 0) return;
	if(isFull(len)) throw buffer_full("mybase::CBufferQueue::enqueue full");

	// 长度字段被分段
	if(_header->iEnd+sizeof(uint64_t) > _header->iBufSize) {
		char tmp[16]; SetLen(tmp,len);
		memcpy(_data+_header->iEnd,tmp,_header->iBufSize-_header->iEnd);
		memcpy(_data,tmp+_header->iBufSize-_header->iEnd,_header->iEnd+sizeof(uint64_t)-_header->iBufSize);
		memcpy(_data+_header->iEnd+sizeof(uint64_t)-_header->iBufSize,buffer,len);
		_header->iEnd = len+_header->iEnd+sizeof(uint64_t)-_header->iBufSize;
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} 
	// 数据被分段
	else if(_header->iEnd+sizeof(uint64_t)+len > _header->iBufSize){
		SetLen(_data+_header->iEnd,len);
		memcpy(_data+_header->iEnd+sizeof(uint64_t),buffer,_header->iBufSize-_header->iEnd-sizeof(uint64_t));
		memcpy(_data,buffer+_header->iBufSize-_header->iEnd-sizeof(uint64_t),len-(_header->iBufSize-_header->iEnd-sizeof(uint64_t)));
		_header->iEnd = len-(_header->iBufSize-_header->iEnd-sizeof(uint64_t));
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} else {
		SetLen(_data+_header->iEnd,len);
		memcpy(_data+_header->iEnd+sizeof(uint64_t),buffer,len);
		_header->iEnd = (_header->iEnd+sizeof(uint64_t)+len)%_header->iBufSize;
	}

       _header->iCount++;
}

void CBufferQueue::enqueue(const char *buffer1,unsigned len1,const char *buffer2,unsigned len2) noexcept(false)
{
	uint64_t len = len1+len2;
	assert(_header);
	assert(_data);
	if(len == 0) return;
	if(isFull(len)) throw buffer_full("mybase::CBufferQueue::enqueue full");

	// 长度字段被分段
	if(_header->iEnd+sizeof(uint64_t) > _header->iBufSize) {
		char tmp[16]; SetLen(tmp,len);
		memcpy(_data+_header->iEnd,tmp,_header->iBufSize-_header->iEnd);
		memcpy(_data,tmp+_header->iBufSize-_header->iEnd,_header->iEnd+sizeof(uint64_t)-_header->iBufSize);
		memcpy(_data+_header->iEnd+sizeof(uint64_t)-_header->iBufSize,buffer1,len1);
		memcpy(_data+_header->iEnd+sizeof(uint64_t)-_header->iBufSize+len1,buffer2,len2);
		_header->iEnd = len+_header->iEnd+sizeof(uint64_t)-_header->iBufSize;
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	}
	// 数据被分段
	else if(_header->iEnd+sizeof(uint64_t)+len > _header->iBufSize){
		SetLen(_data+_header->iEnd,len);
		if(_header->iEnd+sizeof(uint64_t)+len1>_header->iBufSize) { //buffer1被分段
			memcpy(_data+_header->iEnd+sizeof(uint64_t),buffer1,_header->iBufSize-_header->iEnd-sizeof(uint64_t));
			memcpy(_data,buffer1+_header->iBufSize-_header->iEnd-sizeof(uint64_t),len1-(_header->iBufSize-_header->iEnd-sizeof(uint64_t)));
			memcpy(_data+len1-(_header->iBufSize-_header->iEnd-sizeof(uint64_t)),buffer2,len2);
		} else { //buffer2被分段
			memcpy(_data+_header->iEnd+sizeof(uint64_t),buffer1,len1);
			memcpy(_data+_header->iEnd+sizeof(uint64_t)+len1,buffer2,_header->iBufSize-_header->iEnd-sizeof(uint64_t)-len1);
			memcpy(_data,buffer2+_header->iBufSize-_header->iEnd-sizeof(uint64_t)-len1,len2-(_header->iBufSize-_header->iEnd-sizeof(uint64_t)-len1));
		}
		_header->iEnd = len-(_header->iBufSize-_header->iEnd-sizeof(uint64_t));
		assert(_header->iEnd+ReserveLen <= _header->iBegin);
	} else {
		SetLen(_data+_header->iEnd,len);
		memcpy(_data+_header->iEnd+sizeof(uint64_t),buffer1,len1);
		memcpy(_data+_header->iEnd+sizeof(uint64_t)+len1,buffer2,len2);
		_header->iEnd = (_header->iEnd+sizeof(uint64_t)+len)%_header->iBufSize;
	}

       _header->iCount++;
}

bool CBufferQueue::isFull(uint64_t len) const
{
	if(len==0) return false;

	if(_header->iEnd == _header->iBegin) {
		if(len+sizeof(uint64_t)+ReserveLen > _header->iBufSize) return true;
		return false;
	}

	if(_header->iEnd > _header->iBegin) {
		assert(_header->iBegin+sizeof(uint64_t) < _header->iEnd);
		return _header->iBufSize - _header->iEnd + _header->iBegin < sizeof(uint64_t)+len+ReserveLen;
	}

	assert(_header->iEnd+ReserveLen <= _header->iBegin);
	return (_header->iBegin - _header->iEnd < sizeof(uint64_t)+len+ReserveLen);
}

uint32_t CBufferQueue::count() const
{
    return _header ? _header->iCount : 0;
}

bool CBufferQueue::getAllData(char* buf, uint32_t& len)
{
    len = 0 ;
    if( isEmpty() ) {
        return false;
    }

    if(_header->iEnd > _header->iBegin) {
        len = (uint32_t)(_header->iEnd - _header->iBegin) ;
        memcpy(buf,_data+_header->iBegin,len);
    } else {
        uint64_t len1 = _header->iBufSize - _header->iBegin;
        memcpy(buf,_data+_header->iBegin,len1);
        uint64_t len2 = _header->iEnd;
        if( len2 > 0 ) {
            memcpy(buf+len1, _data, len2);
        }

        len = len1 + len2;
    }

    _header->iBegin = 0;
    _header->iEnd = 0;
    _header->iCount = 0;

    return true;
}

bool CLockBufferQueue::init(uint64_t size, bool isCond, bool singleread)
{
    m_size = size;

    m_ptr = new char[size];
    m_queue.create(m_ptr, size);

    m_isCond = isCond;
    m_singleRead = singleread;

    if( m_singleRead ) {
        m_tmpPtr = new char[size];
        m_tmpIndex = m_tmpPtr;
        m_tmpEnd = m_tmpPtr;
    }

    return true;
}
void CLockBufferQueue::enqueue(const char *buffer1,unsigned len1,const char *buffer2,unsigned len2)
{
    // std::unique_lock<std::mutex> lk(m_mutex);
    CGuard<CMutex> guard(m_mutex);

    m_queue.enqueue(buffer1, len1, buffer2, len2);
    if ( m_isCond ) {
        // m_cond.notify_all();
        m_cond.signal();
    }
}

int32_t CLockBufferQueue::enqueue(const CEnqueUnit * unitArray, int32_t num)
{
    int32_t count = 0 ;
    if( num <= 0 ) {
        return count;
    }

    // std::unique_lock<std::mutex> lock(m_mutex);
    CGuard<CMutex> guard(m_mutex);

    try {
	    for(int32_t i = 0 ; i < num; ++i) {
		    const CEnqueUnit* tmpUnit = unitArray+i;
		    if( tmpUnit->buffer2 ) {
		        m_queue.enqueue(tmpUnit->buffer1, tmpUnit->len1, tmpUnit->buffer2, tmpUnit->len2);
		    } else {
		        m_queue.enqueue(tmpUnit->buffer1, tmpUnit->len1);
		    }
		    ++count;
		}
    } catch(...) {
	    if (count > 0 &&  m_isCond ) {
		    // m_cond.notify_one();
		    m_cond.signal();
		}
	    return count;
	}

	if (count > 0 &&  m_isCond ) {
	    // m_cond.notify_one();
	    m_cond.signal();
	}
    return count;
}

// int32_t CLockBufferQueue::dequeue(CEnqueUnit* unitArray, int32_t num, uint64_t msec, bool is_pair)
// {
// 	int32_t count = 0 ;
//     if( num <= 0 ) {
//         return count;
//     }

//     if ( (0 == msec) && m_queue.isEmpty() ) return 0;

//     // std::unique_lock<std::mutex> lock(m_mutex);
//     CGuard<CMutex> guard(m_mutex);
//     if ( m_queue.isEmpty() ) {
//         if ( 0 == msec ) {
//             return 0;
//         } else {
//         	struct timeval tv;
// 		    gettimeofday(&tv, nullptr);
// 		    struct timespec _time;
// 			_time.tv_sec = tv.tv_sec + msec/1000;
// 			_time.tv_nsec = tv.tv_usec*1000 + ( (msec%1000) * 1000 * 1000 );

//             // m_cond.wait_for(lk, std::chrono::milliseconds(msec));
//             m_cond.wait(&_time);
//         }
//     }

//     try {
//     	bool rc = false;
//         for(int32_t i = 0 ; i < num; ++i) {
//         	CEnqueUnit* tmpUnit = unitArray+i;

// 		    if( is_pair ) {
// 		        rc = m_queue.dequeue(tmpUnit->buffer1, tmpUnit->len1, tmpUnit->buffer2, tmpUnit->len2);
// 		    } else {
// 		        rc = m_queue.dequeue(tmpUnit->buffer1, tmpUnit->len1);
// 		    }

// 		    if (!rc) {
// 		    	return count;
// 		    }

// 		    ++count;
// 		}
//     } catch(...) {
//         return count;
//     }

//     return count;
// }

bool CLockBufferQueue::dequeue(char *buffer1, uint32_t& buffersize1, char *buffer2, uint32_t& buffersize2, uint64_t msec)
{
    bool ret = false;
    if( m_tmpPtr ) {
        ret = dequeueFromBuffer(buffer1, buffersize1, buffer2, buffersize2);
        if( ret ) {
            return ret;
        }
    }

    ret = dequeueNoBuffer(buffer1,buffersize1,buffer2,buffersize2, msec);
    if( ret ) {
        getQueueDataToBuffer();
    }

    return ret;

}

bool CLockBufferQueue::dequeueNoBuffer(char *buffer1, uint32_t& buffersize1, char *buffer2, uint32_t& buffersize2, uint64_t msec)
{
    if ( m_queue.isEmpty() ) {
        if ( (0 == msec) || (false == m_isCond) ) {
            return false;
        }
    }

    // std::unique_lock<std::mutex> lk(m_mutex);
    CGuard<CMutex> guard(m_mutex);
    if ( m_queue.isEmpty() ) {
        if ( (0 == msec) || (false == m_isCond) ) {
            return false;
        } else {
        	struct timeval tv;
		    gettimeofday(&tv, nullptr);
		    struct timespec _time;
			_time.tv_sec = tv.tv_sec + msec/1000;
			_time.tv_nsec = tv.tv_usec*1000 + ( (msec%1000) * 1000 * 1000 );

            // m_cond.wait_for(lk, std::chrono::milliseconds(msec));
            m_cond.wait(&_time);
        }
    }

    try
    {
        return m_queue.dequeue(buffer1, buffersize1, buffer2, buffersize2);
    }
    catch(...)
    {
        return false;
    }

    return false;
}

bool CLockBufferQueue::dequeueFromBuffer(char *buffer1, uint32_t& buffersize1, char *buffer2, uint32_t& buffersize2)
{
	if (!m_tmpPtr) return false;

    uint64_t leftlen = m_tmpEnd-m_tmpIndex;
    if( leftlen == 0 ) {
        return false;
    }

    if( leftlen < sizeof(long) ) {
        m_tmpIndex = m_tmpPtr;
        m_tmpEnd = m_tmpPtr;
        return false;
    } else {
       	uint64_t packetlen = 0;
        memcpy(&packetlen, m_tmpIndex, sizeof(long));
        m_tmpIndex += sizeof(long);
        if( m_tmpIndex + packetlen > m_tmpEnd ) {
            m_tmpIndex = m_tmpPtr;
            m_tmpEnd = m_tmpPtr;
            return false;
        } else {
            if( packetlen < buffersize1 ) {
                m_tmpIndex = m_tmpPtr;
                m_tmpEnd = m_tmpPtr;
                return false;
            } else {
                memcpy(buffer1,m_tmpIndex,buffersize1);
                m_tmpIndex += buffersize1;
                if( packetlen - buffersize1 > buffersize2 ) {
                    m_tmpIndex = m_tmpPtr;
                    m_tmpEnd = m_tmpPtr;
                    return false;
                }
                buffersize2 = packetlen - buffersize1;
                memcpy(buffer2,m_tmpIndex,buffersize2);
                m_tmpIndex += buffersize2;

                return true;
            }
        }
    }

    return false;
}

void CLockBufferQueue::enqueue(const char *buffer1, uint32_t len1)
{
    // std::unique_lock<std::mutex> lk(m_mutex);
    CGuard<CMutex> guard(m_mutex);

    m_queue.enqueue(buffer1,len1);
    if ( m_isCond ) {
        // m_cond.notify_all();
        m_cond.signal();
    }
}

bool CLockBufferQueue::dequeue(char *buffer1, uint32_t & buffersize1, uint64_t msec)
{
    if ( m_queue.isEmpty() ) {
        if ( (0 == msec) || (false == m_isCond) ) {
            return false;
        }
    }

    // std::unique_lock<std::mutex> lk(m_mutex);
    CGuard<CMutex> guard(m_mutex);
    if ( m_queue.isEmpty() ) {
        if ( (0 == msec) || (false == m_isCond) ) {
            return false;
        } else {
        	struct timespec _time;
        	_time.tv_sec = msec/1000;
        	_time.tv_nsec = (msec%1000) * 1000 * 1000;
            // m_cond.wait_for(lk, std::chrono::milliseconds(msec));
            m_cond.wait(&_time);
            // m_cond.wait_for(lk, std::chrono::milliseconds(msec));
        }
    }

    try
    {
        return m_queue.dequeue(buffer1,buffersize1);
    }
    catch(...)
    {
        return false;
    }

    return false;
}

bool CLockBufferQueue::isEmpty()
{
    // std::unique_lock<std::mutex> lk(m_mutex);
    CGuard<CMutex> guard(m_mutex);
    return m_queue.isEmpty();
}

bool CLockBufferQueue::getQueueDataToBuffer()
{
	if (!m_tmpPtr) return false;

    // std::unique_lock<std::mutex> lk(m_mutex);
    CGuard<CMutex> guard(m_mutex);

    uint32_t buflen = 0;
    if( m_queue.getAllData(m_tmpPtr, buflen) ) {
        m_tmpIndex = m_tmpPtr;
        m_tmpEnd = m_tmpPtr + buflen;
        return true;
    } else {
        return false;
    }
}

BufferQueue::BufferQueue() : startPos(0)
                           , endPos(0)
                           , totalBufSize(0)
                           , circleBuf(nullptr, _buffer_deleter_)
{
    // throw std::bad_alloc();
}

bool BufferQueue::initialize(uint64_t maxSize) {
    if (0 == maxSize) {
        return false;
    }

    totalBufSize = maxSize;
    circleBuf.reset(new char[maxSize]);

    if (!circleBuf) {
        totalBufSize = 0;
        return false;
    }

    return true;
}

bool BufferQueue::enqueue(const char* buf, uint64_t len) {
    char* buffer = circleBuf.get();

    if(len == 0) {
        return false;
    }
    if(tryEnqueueFull(len)) {
        return false;
    }

    if(endPos + sizeof(uint64_t) > totalBufSize) {
        char tmp[16] = {0};
        setLen(tmp, len);
        memcpy(buffer + endPos, tmp, totalBufSize - endPos);
        memcpy(buffer,
               tmp + totalBufSize - endPos,
               endPos + sizeof(uint64_t) - totalBufSize);
        memcpy(buffer + endPos + sizeof(uint64_t) - totalBufSize, buf, len);

        endPos = len + endPos + sizeof(uint64_t) - totalBufSize;
        assert(endPos <= startPos);
    }
    else if(endPos + sizeof(uint64_t) + len > totalBufSize){
        setLen(buffer + endPos, len);
        memcpy(buffer + endPos + sizeof(uint64_t),
               buf,
               totalBufSize - endPos - sizeof(uint64_t));
        memcpy(buffer,
               buf + totalBufSize - endPos - sizeof(uint64_t),
               len - (totalBufSize - endPos - sizeof(uint64_t)));

        endPos = len - (totalBufSize - endPos - sizeof(uint64_t));
        assert(endPos <= startPos);
    } else {
        setLen(buffer + endPos, len);
        memcpy(buffer + endPos + sizeof(uint64_t), buf, len);

        endPos = (endPos + sizeof(uint64_t) + len) % totalBufSize;
    }

    eleNumber++;
    return true;
}

bool BufferQueue::enqueue(DataBuffer& data)
{
    char* buf = (char*)data.getData();
    uint32_t len = data.getDataLen();

    return enqueue(buf, len);
}

bool BufferQueue::enqueue(const char* buf1, uint64_t len1,
                             const char* buf2, uint64_t len2) {
    uint64_t len = len1 + len2;
    if(len == 0) {
        return false;
    }

    char* buffer = circleBuf.get();
    if(tryEnqueueFull(len)) {
        return false;
    }

    uint64_t encoded_len = ((uint64_t)0x01 << 62) | ( len1 << 32) | len2;
    if(endPos + sizeof(uint64_t) > totalBufSize) {
        char tmp[16] = {0};
        setLen(tmp, encoded_len);
        memcpy(buffer + endPos,
               tmp,
               totalBufSize - endPos);
        memcpy(buffer,
               tmp + totalBufSize - endPos,
               endPos + sizeof(uint64_t) - totalBufSize);
        memcpy(buffer + endPos + sizeof(uint64_t) - totalBufSize,
               buf1,
               len1);
        memcpy(buffer + endPos + sizeof(uint64_t) - totalBufSize + len1,
               buf2,
               len2);

        endPos = len + endPos + sizeof(uint64_t) - totalBufSize;
        assert(endPos <= startPos);
    } else if(endPos + sizeof(uint64_t) + len > totalBufSize){
        setLen(buffer + endPos, encoded_len);
        if(endPos + sizeof(uint64_t) + len1 > totalBufSize) {
            memcpy(buffer + endPos + sizeof(uint64_t),
                   buf1,
                   totalBufSize - endPos - sizeof(uint64_t));
            memcpy(buffer,
                   buf1 + totalBufSize - endPos - sizeof(uint64_t),
                   len1 - (totalBufSize - endPos - sizeof(uint64_t)));
            memcpy(buffer + len1 - (totalBufSize - endPos - sizeof(uint64_t)),
                   buf2,
                   len2);
        } else {
            memcpy(buffer + endPos + sizeof(uint64_t),
                   buf1,
                   len1);
            memcpy(buffer + endPos + sizeof(uint64_t) + len1,
                   buf2,
                   totalBufSize - endPos - sizeof(uint64_t) - len1);
            memcpy(buffer,
                   buf2 + totalBufSize - endPos - sizeof(uint64_t) - len1,
                   len2 - (totalBufSize - endPos - sizeof(uint64_t) - len1));
        }

        endPos = len - (totalBufSize - endPos - sizeof(uint64_t));
        assert(endPos <= startPos);
    } else {
        setLen(buffer + endPos, encoded_len);
        memcpy(buffer + endPos + sizeof(uint64_t),
               buf1,
               len1);
        memcpy(buffer + endPos + sizeof(uint64_t) + len1,
               buf2,
               len2);
        endPos = (endPos + sizeof(uint64_t) + len) % totalBufSize;
    }

    eleNumber++;
    return true;
}

/* 2 | 30 | 32 */
bool BufferQueue::enqueue(DataBuffer& hdr, DataBuffer& data)
{
    const char* buf1 = hdr.getData();
    uint64_t len1 = hdr.getDataLen();
    const char* buf2 = data.getData();
    uint64_t len2 = data.getDataLen();

    return enqueue(buf1, len1, buf2, len2);
}

bool BufferQueue::dequeue(std::string& data)
{
    if(isEmpty()) {
        return false;
    }

    char* buffer = circleBuf.get();
    data.clear();

    if(endPos > startPos) {
        uint64_t encoded_len = getLen(buffer + startPos);
        uint8_t type = (encoded_len >> 62) & 0x03;
        if (type != 0) {
            return false;
        }
        uint64_t length = (encoded_len & (0x3FFFFFFFFFFFFFFF));

        assert(startPos + sizeof(uint64_t) + length <= endPos);
        data.append(buffer + startPos + sizeof(uint64_t), length);
        startPos += length + sizeof(uint64_t);
    } else {
        uint64_t new_begin = 0;
        char *data_from = NULL;
        char *data_to = NULL;
        assert(startPos + 1 <= totalBufSize);
        uint64_t length = 0;

        if(startPos + sizeof(uint64_t) > totalBufSize) { 
            char tmp[16] = {0};
            memcpy(tmp, buffer + startPos, totalBufSize - startPos);
            memcpy(tmp + totalBufSize - startPos,
                   buffer,
                   startPos + sizeof(uint64_t) - totalBufSize);

            uint64_t encoded_len = getLen(tmp);
            uint8_t type = (encoded_len >> 62) & 0x03;
            if (type != 0) {
                return false;
            }
            length = (encoded_len & (0x3FFFFFFFFFFFFFFF));

            data_from = buffer + (startPos + sizeof(uint64_t) - totalBufSize); //
            new_begin = startPos + sizeof(uint64_t) - totalBufSize + length;
            assert(new_begin <= endPos);
        } else {
            uint64_t encoded_len = getLen(buffer + startPos);
            uint8_t type = (encoded_len >> 62) & 0x03;
            if (type != 0) {
                return false;
            }
            length = (encoded_len & (0x3FFFFFFFFFFFFFFF));

            data_from = buffer + startPos + sizeof(uint64_t);
            if(data_from == buffer + totalBufSize) {
                data_from = buffer;
            }
            if(startPos + sizeof(uint64_t) + length < totalBufSize) { 
                new_begin = startPos + sizeof(uint64_t) + length;
            } else {
                new_begin = startPos + sizeof(uint64_t) + length - totalBufSize;
                assert(new_begin <= endPos);
            }
        }
        data_to = buffer + new_begin;
        startPos = new_begin;

        if(data_to > data_from) {
            assert((uint64_t)(data_to - data_from) == length);
            data.append(data_from, length);
        } else {
            data.append(data_from, buffer - data_from + totalBufSize);
            data.append(buffer, data_to - buffer);
            assert(totalBufSize - (data_from - data_to) == length);
        }
    }

    if(eleNumber != 0){
       eleNumber--; 
    }

    return true;
}

bool BufferQueue::dequeue(DataBuffer& data)
{
    if(isEmpty()) {
        return false;
    }

    char* buffer = circleBuf.get();
    data.clear();

    if(endPos > startPos) {
        uint64_t encoded_len = getLen(buffer + startPos);
        uint8_t type = (encoded_len >> 62) & 0x03;
        if (type != 0) {
            return false;
        }
        uint64_t length = (encoded_len & (0x3FFFFFFFFFFFFFFF));

        assert(startPos + sizeof(uint64_t) + length <= endPos);
        data.writeBytes(buffer + startPos + sizeof(uint64_t), length);
        startPos += length + sizeof(uint64_t);
    } else {
        uint64_t new_begin = 0;
        char *data_from = NULL;
        char *data_to = NULL;
        assert(startPos + 1 <= totalBufSize);
        uint64_t length = 0;

        if(startPos + sizeof(uint64_t) > totalBufSize) { 
            char tmp[16] = {0};
            memcpy(tmp, buffer + startPos, totalBufSize - startPos);
            memcpy(tmp + totalBufSize - startPos,
                   buffer,
                   startPos + sizeof(uint64_t) - totalBufSize);

            uint64_t encoded_len = getLen(tmp);
            uint8_t type = (encoded_len >> 62) & 0x03;
            if (type != 0) {
                return false;
            }
            length = (encoded_len & (0x3FFFFFFFFFFFFFFF));

            data_from = buffer + (startPos + sizeof(uint64_t) - totalBufSize); //
            new_begin = startPos + sizeof(uint64_t) - totalBufSize + length;
            assert(new_begin <= endPos);
        } else {
            uint64_t encoded_len = getLen(buffer + startPos);
            uint8_t type = (encoded_len >> 62) & 0x03;
            if (type != 0) {
                return false;
            }
            length = (encoded_len & (0x3FFFFFFFFFFFFFFF));

            data_from = buffer + startPos + sizeof(uint64_t);
            if(data_from == buffer + totalBufSize) {
                data_from = buffer;
            }
            if(startPos + sizeof(uint64_t) + length < totalBufSize) { 
                new_begin = startPos + sizeof(uint64_t) + length;
            } else {
                new_begin = startPos + sizeof(uint64_t) + length - totalBufSize;
                assert(new_begin <= endPos);
            }
        }
        data_to = buffer + new_begin;
        startPos = new_begin;

        if(data_to > data_from) {
            assert((uint64_t)(data_to - data_from) == length);
            data.writeBytes(data_from, length);
        } else {
            data.writeBytes(data_from, buffer - data_from + totalBufSize);
            data.writeBytes(buffer, data_to - buffer);
            assert(totalBufSize - (data_from - data_to) == length);
        }
    }

    if(eleNumber != 0){
       eleNumber--; 
    }

    return true;
}

bool BufferQueue::dequeue(DataBuffer& hdr, DataBuffer& data)
{
    if(isEmpty()) {
        return false;
    }

    char* buffer = circleBuf.get();
    hdr.clear();
    data.clear();

    if(endPos > startPos) {
        assert(startPos + sizeof(uint64_t) < endPos);
        uint64_t encoded_len = getLen(buffer + startPos);
        uint8_t type = (encoded_len >> 62);
        if (type != 1) {
            return false;
        }
        uint64_t len1 = (encoded_len & (0x3FFFFFFF00000000)) >> 32;
        uint64_t len2 = (encoded_len & (0xFFFFFFFF));
        uint64_t len = len1 + len2;

        assert(startPos + sizeof(uint64_t) + len <= endPos);
        hdr.writeBytes(buffer + startPos + sizeof(uint64_t), len1);
        data.writeBytes(buffer + startPos + sizeof(uint64_t) + len1, len2);
        startPos += ( len + sizeof(uint64_t) );
    } else {
        uint64_t len = 0, len1 = 0, len2 = 0;
        uint64_t new_begin = 0;
        char *data_from = NULL;
        char *data_to = NULL;

        assert(startPos + 1 <= totalBufSize);
        if(startPos + sizeof(uint64_t) > totalBufSize) { 
            char tmp[16] = {0};
            memcpy(tmp,
                   buffer + startPos,
                   totalBufSize - startPos);
            memcpy(tmp + totalBufSize - startPos,
                   buffer,
                   startPos + sizeof(uint64_t) - totalBufSize);
            uint64_t encoded_len = getLen(tmp);
            uint8_t type = (encoded_len >> 62) & 0x03;
            if (type != 1) {
                return false;
            }
            len1 = (encoded_len & (0x3FFFFFFF00000000)) >> 32;
            len2 = (encoded_len & (0xFFFFFFFF));
            len = len1 + len2;

            data_from = buffer + (startPos + sizeof(uint64_t) - totalBufSize);
            new_begin = (startPos + sizeof(uint64_t) - totalBufSize) + len;
            assert(new_begin <= endPos);
        } else {
            uint64_t encoded_len = getLen(buffer + startPos);
            uint8_t type = (encoded_len >> 62) & 0x03;
            if (type != 1) {
                return false;
            }
            len1 = (encoded_len & (0x3FFFFFFF00000000)) >> 32;
            len2 = (encoded_len & (0xFFFFFFFF));
            len = len1 + len2;

            data_from = buffer + startPos + sizeof(uint64_t);
            if(data_from == buffer + totalBufSize) {
                data_from = buffer;
            }
            if(startPos + sizeof(uint64_t) + len < totalBufSize) { 
                new_begin = startPos + sizeof(uint64_t) + len;
            } else {
                new_begin = startPos + sizeof(uint64_t) + len - totalBufSize;
                assert(new_begin <= endPos);
            }
        }
        data_to = buffer + new_begin;
        startPos = new_begin;

        if(data_to > data_from) {
            assert((uint64_t)(data_to - data_from) == (uint64_t)len);
            hdr.writeBytes(data_from, len1);
            data.writeBytes(data_from + len1, len2);
        } else {
            assert(totalBufSize - (data_from - data_to) == len);
            if(len1 > buffer - data_from + totalBufSize) {
                hdr.writeBytes(data_from, buffer + totalBufSize - data_from);
                hdr.writeBytes(buffer, len1 - (buffer + totalBufSize - data_from));
                data.writeBytes(data_from + len1 - totalBufSize, len2);
            } else {
                hdr.writeBytes(data_from, len1);
                data.writeBytes(data_from + len1, buffer + totalBufSize - data_from - len1);
                data.writeBytes(buffer, len - (buffer - data_from + totalBufSize));
            }
        }
    }

    if(eleNumber) {
       eleNumber--; 
    }

    return true;
}

bool BufferQueue::isEmpty() {
    return startPos == endPos;
}

bool BufferQueue::tryEnqueueFull(uint64_t len) {
    if( 0 == len ) {
        return false;
    }

    if(endPos == startPos) {
        if(len > totalBufSize) {
            return true;
        }
        return false;
    }

    if(endPos > startPos) {
        uint64_t remain_size = totalBufSize - endPos + startPos;
        return (remain_size < sizeof(uint64_t) + len);
    }

    assert(endPos <= startPos);
    return (startPos - endPos < sizeof(uint64_t) + len);
}

uint64_t BufferQueue::size() {
    if(endPos >= startPos) {
        return endPos - startPos;
    }

    return totalBufSize - (startPos - endPos);
}

uint64_t BufferQueue::count() {
    return eleNumber;
}

uint64_t BufferQueue::getLen(char* buf) {
    uint64_t u;
    memcpy((void *)&u, buf, sizeof(uint64_t));
    return u;
}

void BufferQueue::setLen(char* buf, uint64_t u) {
    memcpy(buf, (void *)&u, sizeof(uint64_t));
}

LockBufferQueue :: LockBufferQueue() : circleBufQueue(nullptr)
{
    // throw std::bad_alloc();
    circleBufQueue.reset(new BufferQueue());
    if (!circleBufQueue) {
        throw std::bad_alloc();
    }
}

bool LockBufferQueue::initialize(uint64_t maxSize) {
    std::unique_lock<std::mutex> l(cvLock);

    return circleBufQueue->initialize(maxSize);
}

bool LockBufferQueue::enqueue(const char* buf, uint64_t len) {
    std::unique_lock<std::mutex> l(cvLock);

    bool rc = circleBufQueue->enqueue(buf, len);
    if (rc) {
        cv.notify_one();
    }

    return rc;
}

bool LockBufferQueue::enqueue(const char* buf1, uint64_t len1,
                                  const char* buf2, uint64_t len2) {
    std::unique_lock<std::mutex> l(cvLock);

    bool rc = circleBufQueue->enqueue(buf1, len1, buf2, len2);
    if (rc) {
        cv.notify_one();
    }

    return rc;
}

bool LockBufferQueue::enqueue(DataBuffer& data)
{
    std::unique_lock<std::mutex> l(cvLock);

    bool rc = circleBufQueue->enqueue(data);
    if (rc) {
        cv.notify_one();
    }

    return rc;
}

bool LockBufferQueue::enqueue(DataBuffer& hdr, DataBuffer& data)
{
    std::unique_lock<std::mutex> l(cvLock);

    bool rc = circleBufQueue->enqueue(hdr, data);
    if (rc) {
        cv.notify_one();
    }

    return rc;
}

bool LockBufferQueue::dequeue(DataBuffer& data, uint64_t wait_ms)
{
    std::unique_lock<std::mutex> l(cvLock);
    if (circleBufQueue->isEmpty()) {
        if ( (0 == wait_ms) ) {
            return false;
        } else {
            cv.wait_for(l, std::chrono::microseconds(wait_ms));
        }
    }

    return circleBufQueue->dequeue(data);
}

bool LockBufferQueue::dequeue(DataBuffer& hdr, DataBuffer& data, uint64_t wait_ms)
{
    std::unique_lock<std::mutex> l(cvLock);
    if (circleBufQueue->isEmpty()) {
        if ( (0 == wait_ms) ) {
            return false;
        } else {
            cv.wait_for(l, std::chrono::microseconds(wait_ms));
        }
    }

    return circleBufQueue->dequeue(hdr, data);
}

bool LockBufferQueue::isEmpty() {
    std::unique_lock<std::mutex> l(cvLock);
    return circleBufQueue->isEmpty();
}

} // namespace