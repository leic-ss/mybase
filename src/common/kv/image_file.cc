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

#include "image_file.h"

#include "binlogrw.h"
#include "asynclimit.h"
#include "common.h"

#include <sstream>

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace mybase
{

ImageEntry::ImageEntry()
{
    bzero(this, getMinLen());

    endSyncTag = EndSyncTag;
    u.s.ver = 1;
}

bool ImageEntry::init(ItemDataInfo* dinfo)
{
    len = Align4(getMinLen() + dinfo->header.keysize + dinfo->header.valsize );
    endSyncTag = EndSyncTag;

    memcpy((void*)&m_metaInfo, (void*)&(dinfo->header), sizeof(m_metaInfo));
    tag = 0;
    memcpy(keyvaluebuf, dinfo->m_data, dinfo->header.keysize + dinfo->header.valsize);

    return true;
}

uint32_t ImageEntry::getMinLen()  
{
    static uint32_t minlen = 0;
    if ( 0 == minlen ) {
        minlen = sizeof(ImageEntry) - sizeof(((ImageEntry*)0)->keyvaluebuf);
    }

    return minlen;
}

void ImageEntry::toKeyValue(DataEntry& key, DataEntry& value)
{
    key.setData(keyvaluebuf, m_metaInfo.keysize, false, true);
    key.data_meta = m_metaInfo;
    key.has_merged = true;

    value.setData(keyvaluebuf + m_metaInfo.keysize, m_metaInfo.valsize, false);
    value.data_meta = m_metaInfo;
}

std::string CImageActHeader::toString()
{
    std::stringstream ss;
    ss << "acctinfo:type[" << type << "] acctnum[" << num << "] filepos[" << pos << "] ";
    ss << "len[" << len << "] jobid[" << u.s.m_jobid << "] m_begin[" << u.s.m_begin <<"] ";
    ss << "m_end[" << u.s.m_end << "]";

    return ss.str();
}

void CImageFileHeader::init(int32_t acctnum)
{
    int32_t len = getHeadLen(acctnum);
    bzero(this,len);

    imageMagic = CImageFileHeaderMagix;
    headlen = len;
    ver = 1;
    acctcount = acctnum;

    for(int32_t i = 0 ; i < acctcount; ++i) {
        getActHeader(i)->type = i;
    }
}

CImageActHeader* CImageFileHeader::getActHeader(int32_t type)
{
    if( type < 0 || type >= acctcount ) {
        return nullptr;
    } else {
        return actHeader + type;
    }
}

std::string CImageFileHeader::toString()
{
    std::stringstream ss;
    ss << "magic[" << imageMagic << "] headlen[" << headlen << "] ver[" << ver << "] ";
    ss << "binlogname[" << binlogname << "] pos[" << pos << "] acctnum[" << acctcount <<"] ";

    for(int32_t i = 0 ; i < acctcount; ++i) {
        ss << "\n";
        ss << actHeader[i].toString();
    }

    return ss.str();
}

bool CImageFileHeader::checkHeaderValid(std::string& err)
{
    if (CImageFileHeader::CImageFileHeaderMagix != imageMagic ) {
        std::stringstream ss;
        ss << "magic[" << imageMagic << "] != " << CImageFileHeader::CImageFileHeaderMagix;
        err = ss.str();
        return false;
    }

    if ( ver != 1 ) {
        std::stringstream ss;
        ss << "current just support ver 1,but image file ver is " << ver;
        err = ss.str();
        return false;
    }
    
    if( acctcount < 1 ) {
        std::stringstream ss;
        ss << "acctnum[" << acctcount << "] < 1";
        err = ss.str();
        return false;
    }

    if( headlen != CImageFileHeader::getHeadLen(acctcount) ) {
        std::stringstream ss;
        ss << "headlen is not correct [" << headlen << " != " << CImageFileHeader::getHeadLen(acctcount) << "]";
        err = ss.str();
        return false;
    }

    return true;
}

CImageLoadDeal::~CImageLoadDeal()
{
    if(m_header) {
        free(m_header);
    }

    if( m_fd >= 0 ) {
        close(m_fd);
        m_fd = -1;
    }
}

bool CImageLoadDeal::init(const char * imagefile)
{
    m_imagefile = imagefile;

    int32_t maxheadlen = 100*1024*1024;
    char buf[4096]={0};

    m_fd = open(m_imagefile.c_str(),O_RDONLY);
    if ( m_fd < 0 ) {
        _log_err(myLog, "open file[%s] error,[%d,%s]", m_imagefile.c_str(), errno, strerror(errno));
        return false;
    }

    int32_t readnum = FileHelper::readn(m_fd, buf, 8) ;
    if ( 8 != readnum ) {
        _log_err(myLog, "read file[%s] head 8 error,just readnum:%d,[%d,%s]",
                 m_imagefile.c_str(), readnum,errno, strerror(errno));
        return false;
    }
    if ( *((uint32_t*)buf) !=  CImageFileHeader::CImageFileHeaderMagix) {
        _log_err(myLog, "read file[%s] head magic is %d != %d",
                 m_imagefile.c_str(), *((uint32_t*)buf), CImageFileHeader::CImageFileHeaderMagix);
        return false;
    }

    int32_t headlen = *((int32_t*)(buf + 4));
    if ( headlen >= maxheadlen ) {
        _log_err(myLog, "read file[%s] read headlen:%d,is too long>[%d]",
                 m_imagefile.c_str(), headlen, maxheadlen);
        return false;
    }

    m_header = (CImageFileHeader*)malloc(headlen);
    if ( !m_header ) {
        _log_err(myLog, "malloc[%d] to fill head fail,[%d:%s]", headlen, errno, strerror(errno));
        return false;
    }
    memcpy(m_header, buf, 8);

    //从头开始重新读取
    if ( 0 != lseek(m_fd, 0, SEEK_SET) ) {
        _log_err(myLog, "lseek file[%s] to head error[%d:%s]", m_imagefile.c_str(),errno,strerror(errno));
        return false;
    }

    readnum = FileHelper::readn(m_fd, m_header, headlen) ;
    if ( headlen != readnum ) {
        _log_err(myLog, "read file[%s] head error,just readnum:%d!=%d,[%d,%s]",
                 m_imagefile.c_str(),readnum,headlen,errno,strerror(errno));
        return false;
    }

    std::string err;
	if( false == m_header->checkHeaderValid(err) ) {
        _log_err(myLog, "checkHeaderValid failed! err: %s", err.c_str());
        return false;
	}

    _log_info(myLog, "%s",m_header->toString().c_str());

    for(int i = 0 ; i < m_header->acctcount; ++i) {
       totalRecordNum += m_header->getActHeader(i)->num;
    }

    _log_info(myLog, "total num: %lu headlen: %d",totalRecordNum, headlen);

    return true;
}

bool CImageLoadDeal::deal()
{
    mybase::BufferFlowWindow flowWindow(m_imagefile, 1024*1024*3);
    flowWindow.setReadPos(m_header->headlen);
    _log_info(myLog, "m_header->headlen: %d", m_header->headlen);

    ImageEntry* image_entry = nullptr;
    uint64_t readcount = 0 ;

    mybase::CAsyncLimit limit(m_rate);

    while( true ) {
        int32_t ret = -1;

        if( m_rate > 0) {
            limit.limitrate();
        }

        ret = flowWindow.readObj((char*&)image_entry);

        if (ret == 0) {
            _log_warn(myLog, "read obj done! ret[%d]", ret);
            break;
        }

        if (ret < 0) {
            _log_err(myLog, "read obj failed! ret[%d]", ret);
            break;
        }

        if (!image_entry) {
            _log_err(myLog, "image buffer is full, should not reach here!");
            return false;
        }

        ++ readcount;
            
        int dealret = 0;
        if (handler) {
            handler(image_entry);
        }

        uint64_t confirmpos = 0;
        flowWindow.releaseObj((char*)image_entry, confirmpos); //处理完就释放

        if( dealret < 0 ) {
            return false;
        }
    }

    if( readcount == totalRecordNum ) {
        _log_info(myLog, "ok num:%lu", readcount);
        sleep(2);
        if(doneHandler) doneHandler();
    } else {
        _log_err(myLog, "statcount:%lu not match readcount:%lu", totalRecordNum, readcount);
        return false;
    }

    return true;
}

}
