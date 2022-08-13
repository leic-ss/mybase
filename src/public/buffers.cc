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

#include "buffers.h"
#include "common.h"

#include <stdlib.h>
#include <assert.h>

namespace mybase {

NBuffer::NBuffer(int32_t maxBufLen) : m_buf(nullptr)
                                    , m_len(0)
                                    , m_index(0)
                                    , m_maxBufLen(maxBufLen)
{
    setNewSize(512);
}

NBuffer::~NBuffer()
{
    m_index = 0;
    FREE(m_buf);
}

void NBuffer::release()
{
    m_index = 0;

    if (m_len > 128 * 1024) {
        setNewSize(512);
    }

    return ;
}

bool NBuffer::copyBuf(const char* buf, int32_t len)
{
    if( len > getFreeLen() ) {
        expand(len);
        if( len > getFreeLen() ) {
            return false;
        }
    }

    memcpy(m_buf+m_index, buf, len);
    m_index += len;

    return true;
}

void NBuffer::expand(int32_t expandLen)
{
    if( expandLen <= 0 ) {
        return ;
    }

    int32_t newLen = m_len;
    do {
        newLen *= 2;
        if (newLen > m_maxBufLen) {       
            newLen = m_maxBufLen;
            break;  
        }       

        if (newLen >= m_len + expandLen) {       
            break;
        }
    } while (true);   

    if (newLen > m_len) {
        setNewSize(newLen);
    }    

    return ;   
}

bool NBuffer::setNewSize(int32_t len)
{
    if( m_len < 0 ) {
        return false;
    }

    if( len > m_len ) {
        return reallocBuf(len);
    } else if ( len == m_len ) {
        return true;
    } else {
        if( len <= m_index ) {
            return false;
        } else {
            return reallocBuf(len);
        }
    }

    return true;
}

bool NBuffer::setMaxBuffLen(uint32_t max_buff_len)
{
    if ((int32_t)max_buff_len < m_index) return false;

    m_maxBufLen = max_buff_len;
    return true;
}

int32_t NBuffer::getFreeLen()
{
    int32_t left =  m_len - m_index;
    if( left < 0 ) {
        left = 0;
    }
    return left;
}

// WARNING: you need ensure safe
void NBuffer::addIndex(int32_t len)
{
    m_index += len;
    if( getFreeLen() < 4096 ) {
        expand(4096);
    }
}

void NBuffer::shrinkFromFront(int32_t len)
{
    int32_t left = m_index - len ;
    if( left > 0 ) {
        memmove(m_buf, m_buf + len, left);
    }
    m_index = left;
}

bool NBuffer::checkCanOp(int32_t len)
{
    if( m_index + len >= m_maxBufLen ) {
        return false;
    }

    return true;
}

bool NBuffer::reallocBuf(int len)
{
    char* newbuf = (char*)calloc(len + 8, 1);
    if( newbuf ) {
        memcpy(newbuf, m_buf, m_index);
        m_len = len;

        FREE(m_buf);
        m_buf = newbuf;

        return true;
    }

    return false;
}


DataBuffer::DataBuffer()
{
    _pend = _pfree = _pdata = _pstart = nullptr;
    _wrapBuf = nullptr;
    _wrapLen = 0;
}

DataBuffer::DataBuffer(uint8_t* wrapbuf, int32_t len)
{
    _wrapBuf = wrapbuf;
    _wrapLen = len;

    _pstart = _wrapBuf;
    _pend = _wrapBuf + _wrapLen;
    _pfree = _wrapBuf;
    _pdata = _wrapBuf;
}

DataBuffer::~DataBuffer()
{
    destroy();
}

void DataBuffer::destroy()
{
    if (!_pstart) return ;

    // be alloced by data buffer, need free
    if( _wrapBuf != _pstart) {
        free(_pstart);
    }
    _pend = _pfree = _pdata = _pstart = nullptr;
}

char* DataBuffer::getData()
{
    return (char*)_pstart;
}

char* DataBuffer::getCurData()
{
    return (char*)_pdata;
}

uint32_t DataBuffer::getDataLen()
{
    return static_cast<uint32_t>(_pfree - _pstart);
}

char* DataBuffer::getFree()
{
    return (char*)_pfree;
}

int32_t DataBuffer::getFreeLen()
{
    return static_cast<int32_t>(_pend - _pfree);
}

void DataBuffer::drainData(int32_t len)
{
    _pdata += len;
    if (_pdata >= _pfree) {
        clear();
    }
}

void DataBuffer::pourData(int32_t len)
{
    assert(_pend - _pfree >= len);
    _pfree += len;
}

void DataBuffer::stripData(int len)
{
    assert(_pfree - _pdata >= len);
    _pfree -= len;
}

bool DataBuffer::pos(uint32_t p)
{
    uint32_t dLen = _pfree - _pstart;
    if (p >= dLen) {
        return false;
    }

    _pdata = _pstart + p;
    return true;
}

void DataBuffer::clear()
{
    _pdata = _pfree = _pstart;
}

bool DataBuffer::expect(int32_t len)
{
    if (_pdata + len > _pfree) {
        return false;
    }

    return true;
}

bool DataBuffer::shrink()
{
    if (_pstart == nullptr) {
        return false;
    }

    if ( (_pend - _pstart) <= DEFAULT_MAX_DATABUFFER_SIZE || 
         (_pfree - _pdata) > DEFAULT_MAX_DATABUFFER_SIZE ) {
        return false;
    }

    int32_t dlen = static_cast<int32_t>(_pfree - _pdata);
    if (dlen < 0) dlen = 0;

    uint8_t *newbuf = (uint8_t*)malloc(DEFAULT_MAX_DATABUFFER_SIZE);
    assert(newbuf != NULL);
    if (dlen > 0) {
        memcpy(newbuf, _pdata, dlen);
    }

    // be alloced by user, need free by user
    if ( _wrapBuf != _pstart ) {
        free(_pstart);
    }

    _pdata = _pstart = newbuf;
    _pfree = _pstart + dlen;
    _pend = _pstart + DEFAULT_MAX_DATABUFFER_SIZE;
    return true;
}

void DataBuffer::writeInt8(uint8_t n)
{
    expand(1);
    *_pfree++ = (uint8_t)n;
}

void DataBuffer::writeInt16(uint16_t n)
{
    expand(2);
    _pfree[1] = (uint8_t)n;
    n = static_cast<uint16_t>(n >> 8);
    _pfree[0] = (uint8_t)n;
    _pfree += 2;
}

void DataBuffer::writeInt32(uint32_t n)
{
    expand(4);
    _pfree[3] = (uint8_t)n;
    n >>= 8;
    _pfree[2] = (uint8_t)n;
    n >>= 8;
    _pfree[1] = (uint8_t)n;
    n >>= 8;
    _pfree[0] = (uint8_t)n;
    _pfree += 4;
}

void DataBuffer::writeInt64(uint64_t n)
{
    expand(8);
    _pfree[7] = (uint8_t)n;
    n >>= 8;
    _pfree[6] = (uint8_t)n;
    n >>= 8;
    _pfree[5] = (uint8_t)n;
    n >>= 8;
    _pfree[4] = (uint8_t)n;
    n >>= 8;
    _pfree[3] = (uint8_t)n;
    n >>= 8;
    _pfree[2] = (uint8_t)n;
    n >>= 8;
    _pfree[1] = (uint8_t)n;
    n >>= 8;
    _pfree[0] = (uint8_t)n;
    _pfree += 8;
}

void DataBuffer::writeBytes(const void *src, int32_t len)
{
    expand(len);
    memcpy(_pfree, src, len);
    _pfree += len;
}

void DataBuffer::fillInt8(uint8_t *dst, uint8_t n)
{
    *dst = n;
}

void DataBuffer::fillInt16(uint8_t *dst, uint16_t n)
{
    dst[1] = (uint8_t)n;
    n = static_cast<uint16_t>(n >> 8);
    dst[0] = (uint8_t)n;
}

void DataBuffer::fillInt32(uint8_t *dst, uint32_t n)
{
    dst[3] = (uint8_t)n;
    n >>= 8;
    dst[2] = (uint8_t)n;
    n >>= 8;
    dst[1] = (uint8_t)n;
    n >>= 8;
    dst[0] = (uint8_t)n;
}

void DataBuffer::fillInt64(uint8_t *dst, uint64_t n)
{
    dst[7] = (uint8_t)n;
    n >>= 8;
    dst[6] = (uint8_t)n;
    n >>= 8;
    dst[5] = (uint8_t)n;
    n >>= 8;
    dst[4] = (uint8_t)n;
    n >>= 8;
    dst[3] = (uint8_t)n;
    n >>= 8;
    dst[2] = (uint8_t)n;
    n >>= 8;
    dst[1] = (uint8_t)n;
    n >>= 8;
    dst[0] = (uint8_t)n;
}

void DataBuffer::writeString(const char *str)
{
    int32_t len = (str ? static_cast<int32_t>(strlen(str)) : 0);
    if (len > 0) len++;
    expand(static_cast<int32_t>(len + sizeof(uint32_t)));

    writeInt32(len);
    writeBytes(str, len);
}

void DataBuffer::writeString(const std::string& str)
{
    int32_t len = str.size();
    expand(static_cast<int32_t>(len+sizeof(uint32_t)));

    writeInt32(len);
    writeBytes(str.data(), len);
}

void DataBuffer::writeVector(const std::vector<int32_t>& v)
{
    const uint32_t iLen = static_cast<uint32_t>(v.size());
    writeInt32(iLen);
    for (uint32_t i = 0; i < iLen; ++i) {
         writeInt32(v[i]);
    }
}

void DataBuffer::writeVector(const std::vector<uint32_t>& v)
{
    const uint32_t iLen = static_cast<uint32_t>(v.size());
    writeInt32(iLen);
    for (uint32_t i = 0; i < iLen; ++i) {
         writeInt32(v[i]);
    } 
}

void DataBuffer::writeVector(const std::vector<int64_t>& v)
{
    const uint32_t iLen = static_cast<uint32_t>(v.size());
    writeInt32(iLen);
    for (uint32_t i = 0; i < iLen; ++i) {
         writeInt64(v[i]);
    }
}

void DataBuffer::writeVector(const std::vector<uint64_t>& v)
{
    const uint32_t iLen = static_cast<uint32_t>(v.size());
    writeInt32(iLen);
    for (uint32_t i = 0; i < iLen; ++i) {
         writeInt64(v[i]);
    }
}

uint8_t DataBuffer::readInt8()
{
    assert(_pdata + 1 <= _pfree);
    return (*_pdata++);
}

uint16_t DataBuffer::readInt16()
{
    assert(_pdata + 2 <= _pfree);
    uint16_t n = _pdata[0];
    n = static_cast<uint16_t>(n << 8);
    n = static_cast<uint16_t>(n | _pdata[1]);
    _pdata += 2;
    return n;
}

uint32_t DataBuffer::readInt32()
{
    assert(_pdata + 4 <= _pfree);
    uint32_t n = _pdata[0];
    n <<= 8;
    n |= _pdata[1];
    n <<= 8;
    n |= _pdata[2];
    n <<= 8;
    n |= _pdata[3];
    _pdata += 4;
    assert(_pfree>=_pdata);
    return n;
}

uint64_t DataBuffer::readInt64()
{
    assert(_pdata + 8 <= _pfree);
    uint64_t n = _pdata[0];
    n <<= 8;
    n |= _pdata[1];
    n <<= 8;
    n |= _pdata[2];
    n <<= 8;
    n |= _pdata[3];
    n <<= 8;
    n |= _pdata[4];
    n <<= 8;
    n |= _pdata[5];
    n <<= 8;
    n |= _pdata[6];
    n <<= 8;
    n |= _pdata[7];
    _pdata += 8;
    assert(_pfree>=_pdata);
    return n;
}

bool DataBuffer::readBytes(void *dst, int32_t len)
{
    if (_pdata + len > _pfree) {
        return false;
    }
    memcpy(dst, _pdata, len);
    _pdata += len;
    assert(_pfree>=_pdata);
    return true;
}

bool DataBuffer::readString(std::string& str)
{
    if (_pdata + sizeof(int32_t) > _pfree) {
        return false;
    }
    int32_t slen = readInt32();
    if (_pfree - _pdata < slen) {
        slen = static_cast<int32_t>(_pfree - _pdata);
    }

    if (slen > 0) {
        str.append((char*)_pdata, (uint64_t)slen);
    }
    _pdata += slen;
    assert(_pfree >= _pdata);
    return true;
}

bool DataBuffer::readString(char *&str, int32_t len)
{
    if (_pdata + sizeof(int32_t) > _pfree) {
        return false;
    }
    int32_t slen = readInt32();
    if (_pfree - _pdata < slen) {
        slen = static_cast<int32_t>(_pfree - _pdata);
    }
    if (str == nullptr && slen > 0) {
        str = (char*)malloc(slen);
        len = slen;
    }

    if (len > slen) {
        len = slen;
    }
    if (len > 0) {
        memcpy(str, _pdata, len);
        str[len-1] = '\0';
    }
    _pdata += slen;
    assert(_pfree >= _pdata);
    return true;
}

bool DataBuffer::readVector(std::vector<int32_t>& v)
{
     const uint32_t len = readInt32();
     for (uint32_t i = 0; i < len; ++i) {
         v.push_back(readInt32());
     }
     return true; 
}

bool DataBuffer::readVector(std::vector<uint32_t>& v)
{
     const uint32_t len = readInt32();
     for (uint32_t i = 0; i < len; ++i) {
         v.push_back(readInt32());
     }
     return true; 
}

bool DataBuffer::readVector(std::vector<int64_t>& v)
{
     const uint32_t len = readInt32();
     for (uint32_t i = 0; i < len; ++i) {
         v.push_back(readInt64());
     }
     return true; 
}

bool DataBuffer::readVector(std::vector<uint64_t>& v)
{
     const uint32_t len = readInt32();
     for (uint32_t i = 0; i < len; ++i) {
         v.push_back(readInt64());
     }
     return true; 
}

void DataBuffer::ensureFree(int32_t len)
{
    expand(len);
}

int32_t DataBuffer::findBytes(const char *findstr, int32_t len)
{
    int32_t dLen = static_cast<int32_t>(_pfree - _pstart - len + 1);
    for (int32_t i = 0; i < dLen; i++) {
        if (_pdata[i] == findstr[0] && memcmp(_pdata+i, findstr, len) == 0) {
            return i;
        }
    }
    return -1;
}

void DataBuffer::expand(int32_t need)
{
    if (_pstart == nullptr) {
        int32_t len = 256;
        while (len < need) {
            len <<= 1;
        }
        _pfree = _pdata = _pstart = (uint8_t*)malloc(len);
        _pend = _pstart + len;
    } else if (_pend - _pfree < need) {
        int32_t flen = static_cast<int32_t>(_pend - _pfree);
        int32_t dlen = static_cast<int32_t>(_pfree - _pstart);

        if (flen < need || flen * 4 < dlen) {
            int32_t bufsize = static_cast<int32_t>((_pend - _pstart) * 2);
            while (bufsize - dlen < need) {
                bufsize <<= 1;
            }

            uint8_t* newbuf = (uint8_t *)malloc(bufsize);
            if (newbuf == nullptr) {
                // do nothing
            }
            assert(newbuf != nullptr);
            if (dlen > 0) {
                memcpy(newbuf, _pstart, dlen);
            }
            free(_pstart);

            uint32_t read_len = (_pdata - _pstart);
            _pstart = newbuf;
            _pdata = _pstart + read_len;

            _pfree = _pstart + dlen;
            _pend = _pstart + bufsize;
        } else {
            uint32_t read_len = (_pdata - _pstart);

            memmove(_pstart, _pdata, dlen);
            _pfree = _pstart + dlen;
            _pdata = _pstart + read_len;
        }
    }
}

CBufferMgn& CBufferMgn::operator=(const CBufferMgn& lft )
{
    deepCopy(lft);
    return *this;
}

void CBufferMgn::append(char ch)
{
    if ( m_pos + 5 >= m_size ) {
        int32_t oldsize = m_size;
        m_size *= 2;
        if ( !m_size ) {
            m_size = 128;
        }

        char* tmpbuf = new char[m_size];
        bzero(tmpbuf, m_size);
        if ( m_buf ) {
            memcpy(tmpbuf, m_buf, oldsize);
            delete[] m_buf;
        }
        m_buf = tmpbuf;
    }

    m_buf[m_pos++] = ch;
    return ;
}

void CBufferMgn::append(const char * str, int32_t len)
{
    if( !str ) {
        return ;
    } 
    if( 0 == len ) {
        len = strlen(str);
    }
    if ( len <= 0 ) {
        return ;
    }
    
    if ( m_pos + len + 5 >= m_size ) {
        int32_t oldsize = m_size;
        m_size = 2 * (m_size+len);          

        char * tmpbuf = new char[m_size];
        bzero(tmpbuf, m_size);
        if ( m_buf ) {
            memcpy(tmpbuf, m_buf, oldsize);
            delete [] m_buf;
        }
        m_buf = tmpbuf;
    }

    memcpy(m_buf+m_pos,str,len);
    m_pos += len;
}

void CBufferMgn::deepCopy(const CBufferMgn& lft)
{
    release();
    append(lft.c_str(), lft.getLen());
}

void CBufferMgn::release()
{
    delete[] m_buf;
    m_buf = nullptr;
    m_pos = 0;
    m_size = 0;
    bzero(m_md5,sizeof(m_md5));
}

bool CBufferMgn::writeToFile(const char* filename)
{
    bool ret = false;
    if ( !m_buf ) {
        _log_err(myLog, "m_buf is empty");
        return ret;
    }

    char tmpname[256];
    char name[256];
    snprintf(tmpname, sizeof(tmpname), "%s.dbconf.tmp", filename);
    snprintf(name, sizeof(name), "%s.dbconf", filename);

    int32_t fd = open(tmpname, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    if ( fd < 0 ) {
        _log_err(myLog, "open file[%s] error[%d:%s]", tmpname, errno, strerror(errno));
        return ret;
    }

    int32_t writenum = write(fd,m_buf,m_pos);
    if ( m_pos != writenum ) {
        _log_err(myLog, "write file[%s] error[%d:%s][%d!=%d]", tmpname, errno, strerror(errno), m_pos, writenum);
    } else {
        ret = true;
    }

    close(fd);

    if( ret && rename(tmpname,name) != 0 ) {
        _log_err(myLog, "rename file[%s->%s] error[%d:%s]", tmpname, name, errno, strerror(errno));
        ret =false;
    }

    return true;
}

bool CBufferMgn::readFromFile(const char* filename)
{
    m_pos = 0 ;
    int32_t fd = open(filename,O_RDONLY);
    if ( fd < 0 ) {
        _log_err(myLog, "open file[%s] error[%d:%s]", filename, errno, strerror(errno));
        return false;
    }

    char buf[4096]={0};
    int32_t readlen = 0 ;
    bool ret = true;
    while( (readlen = read(fd, buf, sizeof(buf))) > 0 ) {
       append(buf, readlen);
    }

    if( readlen < 0 ) {
       _log_err(myLog, "read file[%s],fd:%d error[%d:%s]", filename, fd, errno, strerror(errno));
       ret = false;
    }

    if( 0 != close(fd) ) {
        _log_err(myLog, "close file[%s],fd:%d error[%d:%s]", filename,fd, errno, strerror(errno));
    }

    return ret;
}

Buffer::Buffer(uint8_t* wrapbuf, int32_t len)
{
    _wrapBuf = wrapbuf;
    _wrapLen = len;

    _pstart = _wrapBuf;
    _pend = _wrapBuf + _wrapLen;
    _pfree = _wrapBuf + _wrapLen;
    _pdata = _wrapBuf;
}

Buffer::~Buffer()
{
    if (!_pstart) return ;

    // be alloced by data buffer, need freeData
    if( _wrapBuf != _pstart) {
        ::free(_pstart);
    }
    _pend = _pfree = _pdata = _pstart = nullptr;
}

std::shared_ptr<Buffer> Buffer::alloc(const int32_t size)
{
    std::shared_ptr<Buffer> ptr(new Buffer());
    ptr->expand(size);

    return ptr;
}

char* Buffer::data()
{
    return (char*)_pstart;
}

int32_t Buffer::dataLen()
{
    return static_cast<int32_t>(_pfree - _pstart);
}

char* Buffer::curData()
{
    return (char*)_pdata;
}

int32_t Buffer::curDataLen()
{
    return static_cast<int32_t>(_pfree - _pdata);
}

char* Buffer::freeData()
{
    return (char*)_pfree;
}

int32_t Buffer::freeLen()
{
    return static_cast<int32_t>(_pend - _pfree);
}

bool Buffer::pos(uint32_t p)
{
    uint32_t dLen = _pfree - _pstart;
    if (p >= dLen) {
        return false;
    }

    _pdata = _pstart + p;
    return true;
}

bool Buffer::pourData(uint32_t len)
{
    if ( (_pend - _pfree) < len) return false;

    _pfree += len;
    return true;
}

void Buffer::writeInt16(uint16_t n)
{
    expand(2);
    _pfree[1] = (uint8_t)n;
    n = static_cast<uint16_t>(n >> 8);
    _pfree[0] = (uint8_t)n;
    _pfree += 2;
}

void Buffer::writeInt32(uint32_t n)
{
    expand(4);
    _pfree[3] = (uint8_t)n;
    n >>= 8;
    _pfree[2] = (uint8_t)n;
    n >>= 8;
    _pfree[1] = (uint8_t)n;
    n >>= 8;
    _pfree[0] = (uint8_t)n;
    _pfree += 4;
}

void Buffer::writeInt64(uint64_t n)
{
    expand(8);
    _pfree[7] = (uint8_t)n;
    n >>= 8;
    _pfree[6] = (uint8_t)n;
    n >>= 8;
    _pfree[5] = (uint8_t)n;
    n >>= 8;
    _pfree[4] = (uint8_t)n;
    n >>= 8;
    _pfree[3] = (uint8_t)n;
    n >>= 8;
    _pfree[2] = (uint8_t)n;
    n >>= 8;
    _pfree[1] = (uint8_t)n;
    n >>= 8;
    _pfree[0] = (uint8_t)n;
    _pfree += 8;
}

void Buffer::writeBytes(const void *src, int32_t len)
{
    expand(len);
    memcpy(_pfree, src, len);
    _pfree += len;
}

void Buffer::writeString(const std::string& str)
{
    int32_t len = str.size();
    expand(static_cast<int32_t>(len+sizeof(uint32_t)));

    writeInt32(len);
    writeBytes(str.data(), len);
}

void Buffer::fillInt8(uint8_t *dst, uint8_t n)
{
    *dst = n;
}

void Buffer::fillInt16(uint8_t *dst, uint16_t n)
{
    dst[1] = (uint8_t)n;
    n = static_cast<uint16_t>(n >> 8);
    dst[0] = (uint8_t)n;
}

void Buffer::fillInt32(uint8_t *dst, uint32_t n)
{
    dst[3] = (uint8_t)n;
    n >>= 8;
    dst[2] = (uint8_t)n;
    n >>= 8;
    dst[1] = (uint8_t)n;
    n >>= 8;
    dst[0] = (uint8_t)n;
}

void Buffer::fillInt64(uint8_t *dst, uint64_t n)
{
    dst[7] = (uint8_t)n;
    n >>= 8;
    dst[6] = (uint8_t)n;
    n >>= 8;
    dst[5] = (uint8_t)n;
    n >>= 8;
    dst[4] = (uint8_t)n;
    n >>= 8;
    dst[3] = (uint8_t)n;
    n >>= 8;
    dst[2] = (uint8_t)n;
    n >>= 8;
    dst[1] = (uint8_t)n;
    n >>= 8;
    dst[0] = (uint8_t)n;
}

uint8_t Buffer::readInt8()
{
    assert(_pdata + 1 <= _pfree);
    return (*_pdata++);
}

uint16_t Buffer::readInt16()
{
    assert(_pdata + 2 <= _pfree);
    uint16_t n = _pdata[0];
    n = static_cast<uint16_t>(n << 8);
    n = static_cast<uint16_t>(n | _pdata[1]);
    _pdata += 2;
    return n;
}

uint32_t Buffer::readInt32()
{
    assert(_pdata + 4 <= _pfree);
    uint32_t n = _pdata[0];
    n <<= 8;
    n |= _pdata[1];
    n <<= 8;
    n |= _pdata[2];
    n <<= 8;
    n |= _pdata[3];
    _pdata += 4;
    assert(_pfree>=_pdata);
    return n;
}

uint64_t Buffer::readInt64()
{
    assert(_pdata + 8 <= _pfree);
    uint64_t n = _pdata[0];
    n <<= 8;
    n |= _pdata[1];
    n <<= 8;
    n |= _pdata[2];
    n <<= 8;
    n |= _pdata[3];
    n <<= 8;
    n |= _pdata[4];
    n <<= 8;
    n |= _pdata[5];
    n <<= 8;
    n |= _pdata[6];
    n <<= 8;
    n |= _pdata[7];
    _pdata += 8;
    assert(_pfree>=_pdata);
    return n;
}

bool Buffer::readBytes(void *dst, int32_t len)
{
    if (_pdata + len > _pfree) {
        return false;
    }
    memcpy(dst, _pdata, len);
    _pdata += len;
    assert(_pfree>=_pdata);
    return true;
}

bool Buffer::readString(std::string& str)
{
    if (_pdata + sizeof(int32_t) > _pfree) {
        return false;
    }
    int32_t slen = readInt32();
    if (_pfree - _pdata < slen) {
        slen = static_cast<int32_t>(_pfree - _pdata);
    }

    if (slen > 0) {
        str.append((char*)_pdata, (uint64_t)slen);
    }
    _pdata += slen;
    assert(_pfree >= _pdata);
    return true;
}

void Buffer::expand(int32_t need)
{
    if (_pstart == nullptr) {
        int32_t len = 256;
        while (len < need) {
            len <<= 1;
        }
        _pfree = _pdata = _pstart = (uint8_t*)malloc(len);
        _pend = _pstart + len;
    } else if (_pend - _pfree < need) {
        int32_t flen = static_cast<int32_t>(_pend - _pfree);
        int32_t dlen = static_cast<int32_t>(_pfree - _pstart);

        if (flen < need || flen * 4 < dlen) {
            int32_t bufsize = static_cast<int32_t>((_pend - _pstart) * 2);
            while (bufsize - dlen < need) {
                bufsize <<= 1;
            }

            uint8_t* newbuf = (uint8_t *)malloc(bufsize);
            if (newbuf == nullptr) {
                // do nothing
            }
            assert(newbuf != nullptr);
            if (dlen > 0) {
                memcpy(newbuf, _pstart, dlen);
            }
            ::free(_pstart);

            uint32_t read_len = (_pdata - _pstart);
            _pstart = newbuf;
            _pdata = _pstart + read_len;

            _pfree = _pstart + dlen;
            _pend = _pstart + bufsize;
        } else {
            uint32_t read_len = (_pdata - _pstart);

            memmove(_pstart, _pdata, dlen);
            _pfree = _pstart + dlen;
            _pdata = _pstart + read_len;
        }
    }
}

}
