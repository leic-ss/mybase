#include "walstore.h"
#include "md5_encode.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

namespace walstore {

static uint64_t sMaxLogIndex = 0xFFFFFFFFFFFFFFFF;

static const int32_t sWriteLockStatus = -1;
static const int32_t sFreeStatus = 0;
static const std::thread::id NullThread;

static std::vector<std::string> tokenize(const std::string& src, const std::string& delim)
{
    std::vector<std::string> ret;
    size_t last = 0;
    size_t pos = src.find(delim, last);
    while (pos != std::string::npos) {
        ret.push_back( src.substr(last, pos - last) );
        last = pos + delim.size();
        pos = src.find(delim, last);
    }
    if (last < src.size()) {
        ret.push_back( src.substr(last) );
    }
    return std::move(ret);
}

void ReadWriteLock::readLock()
{
    std::unique_lock<std::mutex> lk(mtx);
    while (count < 0 || writeWaitCount > 0) {
        cond.wait(lk);
    }
    ++count;
}

void ReadWriteLock::readUnlock()
{
    std::unique_lock<std::mutex> lk(mtx);
    if (--count == 0) {
        cond.notify_one();
    }
}

void ReadWriteLock::writeLock()
{
    std::unique_lock<std::mutex> lk(mtx);
    writeWaitCount++;
    while (count != 0) {
        cond.wait(lk);
    }

    writeWaitCount--;
    count = -1;
}

void ReadWriteLock::writeUnlock()
{
    std::unique_lock<std::mutex> lk(mtx);
    count = 0;
    cond.notify_all();
}

RWLock::RWLock(bool write_first)
    		  : writeFirst(write_first)
    		  , writeThreadId()
{
}

// > 0: success
int32_t RWLock::readLock() {
    if (std::this_thread::get_id() != writeThreadId) {
        int32_t count;
        if (writeFirst) {
        	do {
                while ((count = lockCount) == sWriteLockStatus || writeWaitCount > 0);//写锁定时等待
            } while (!lockCount.compare_exchange_weak(count, count + 1));
        } else {
        	do {
                while ((count = lockCount) == sWriteLockStatus); //写锁定时等待
            } while (!lockCount.compare_exchange_weak(count, count + 1));
        }
    }
    return lockCount;
}

int32_t RWLock::readUnlock() {
    // ==时为独占写状态,不需要加锁
    if (std::this_thread::get_id() != writeThreadId) {
    	--lockCount;
    }
    return lockCount;
}

// -1: success
int32_t RWLock::writeLock()
{
    if (std::this_thread::get_id() != writeThreadId){
        writeWaitCount++;
        for(int32_t zero=sFreeStatus; !lockCount.compare_exchange_weak(zero, sWriteLockStatus); zero=sFreeStatus);
        writeWaitCount--;
        writeThreadId=std::this_thread::get_id();
    }

    return lockCount;
}

int32_t RWLock::writeUnlock()
{
    if(std::this_thread::get_id() != writeThreadId) {
        assert(false);
    }
    assert(sWriteLockStatus==lockCount);

    writeThreadId=NullThread;
    lockCount.store(sFreeStatus);
    return lockCount;
}

std::shared_ptr<Buffer> WalFileInfo::buildIndex()
{
	std::shared_ptr<Buffer> buf = Buffer::alloc(1024);
	buf->writeInt32(ivts.size());
	for (auto& ele : ivts) {
		buf->writeInt64(ele.first);
		buf->writeInt64(ele.second);
	}

	return buf;
}

bool WalFileInfo::insert(uint64_t seq, uint64_t size)
{
	rwlock.writeLock();
	if (!isNormal()) {
		if (!loadInfo()) {
			rwlock.writeUnlock();
			return false;
		}
		setNormal();
	}

	ivts.emplace(seq, size);
	rwlock.writeUnlock();
	return true;
}

uint64_t WalFileInfo::getIvtIndex(uint64_t seq)
{
	uint64_t value = 0;

	rwlock.readLock();
	if (!isNormal()) {
		if (!loadInfo()) {
			rwlock.readUnlock();
			return 0;
		}
		setNormal();
	}

	auto iter = ivts.find(seq);
    if (iter != ivts.end()) {
    	value = iter->second;
    }
	rwlock.readUnlock();

	return value;
}

uint32_t WalFileInfo::rollback(uint64_t idx)
{
	if (idx < startIndex || idx > lastIndex) return 0;

	for (uint64_t i = idx + 1; i <= lastIndex; i++) {
		ivts.erase(i);
	}

	auto iter = ivts.find(idx);
	if (iter == ivts.end()) return 0;

	uint64_t ivt = iter->second;
	uint32_t pos = (ivt >> 32) & (uint32_t)0xFFFFFFFF;
	uint32_t length = ((uint32_t)(ivt) & (uint32_t)0xFFFFFFFF);

	uint32_t last_pos = pos + length;
	if(fd > 0) ftruncate(fd, last_pos);

	return last_pos;
}

void WalFileInfo::clean()
{
	rwlock.writeLock();

	ivts.clear();
	setClean();

	rwlock.writeUnlock();
}

WalFileInfo::WalFileInfo(std::string path, int64_t start_idx)
		                 : fullPath(path)
		     			 , startIndex(start_idx)
{
	fd = open(path.c_str(), O_RDWR | O_CREAT | O_APPEND | O_LARGEFILE, 0644);
}

WalFileInfo::~WalFileInfo()
{
	if (fd > 0) close(fd);
}


bool WalFileInfo::loadInfo()
{
	uint64_t total_size = FileUtils::fileSize(fullPath.c_str());
	uint64_t length = 73;

	do {
		if (total_size <= length) break;

		// compare md5
		std::shared_ptr<Buffer> buf = Buffer::alloc(length);
		uint64_t read_len = pread(fd, buf->data(), buf->freeLen(), total_size - length);
		if ( read_len != length) {
	      	return false;
	    }
	    buf->pourData(length);

	    char md5_buf[8] = {0};
	    buf->readBytes(md5_buf, sizeof(md5_buf));

	    char md5_calc[65] = {0};
	    MD5AndEncode((uint8_t*)md5_buf, sizeof(md5_buf), (uint8_t*)md5_calc);

		char md5_rec[65] = {0};
		buf->readBytes(md5_rec, sizeof(md5_rec));

		// TODO: may have a risk, MD5 compare
		if ( memcmp(md5_calc, md5_rec, sizeof(md5_rec)) != 0 ) {
			fprintf(stderr, "md5 mismatch!\n");
			break;
		}

		Buffer buf2((uint8_t*)md5_buf, sizeof(md5_buf));
		uint32_t pos = buf2.readInt32();
		uint32_t size = buf2.readInt32();

		buf = Buffer::alloc(size);
		read_len = pread(fd, buf->data(), size, pos);
		if ( read_len != size) {
	      	return false;
	    }
	    buf->pourData(size);

	    uint32_t count = buf->readInt32();
	    for (uint32_t i = 0; i < count; i++) {
	    	uint64_t seq = buf->readInt64();
	    	uint64_t idx = buf->readInt64();
	    	ivts.emplace(seq, idx);

	    	if (lastIndex < seq) lastIndex = seq;
	    }

	    loadFull = true;
	    return true;
	} while(false);

	uint64_t last_idx = startIndex;
	uint32_t pos = 0;
	while (pos < total_size) {
		uint32_t header_size = 4;
		std::shared_ptr<Buffer> buf = Buffer::alloc(header_size);
		uint64_t read_len = pread(fd, buf->data(), header_size, pos);
		if ( read_len != header_size) {
	      	return false;
	    }
	    buf->pourData(header_size);
	    uint32_t data_size = buf->readInt32();

	    uint32_t vsize = (header_size + data_size);
	    uint64_t size = ((uint64_t)pos << 32) | (uint64_t)vsize;
		ivts.emplace(last_idx, size);

	    pos += vsize;
	    lastIndex = last_idx++;
	}

	return true;
}

WalStore::WalStore(std::shared_ptr<raftcc::logger> logger_) : myLog(logger_)
{ }

WalStore::~WalStore()
{ }

bool SortByString( const std::string& v1, const std::string& v2)
{  
    return v1 < v2;
}

bool WalStore::scanAllWalFiles()
{
	std::vector<std::string> files = FileUtils::listAllFilesInDir(walDir.c_str(), false, "*.wal");
	std::sort(files.begin(), files.end(), SortByString);

	for (uint32_t i = 0; i < files.size(); i++) {
		std::string fn = files[i];
		std::vector<std::string> parts = tokenize(fn, ".");
	    if (parts.size() != 2) {
		    continue;
	    }

	    int64_t start_log_id = atol(parts[0].c_str());
	    if (start_log_id == 0) continue;

	    if (startIdx == 0) startIdx = start_log_id;

	    curWalFile.reset();
	    std::string path = FileUtils::joinPath(walDir, fn);

	    curWalFile = std::make_shared<WalFileInfo>(path, start_log_id);
	    int32_t rfd = curWalFile->getFd();

	    // TODO: skip loading with some conditions
	    {
	    	bool rc = curWalFile->loadInfo();
			if (!rc) {
				_log_err(myLog, "load info failed! fn[%s] start_log_id[%lu] last_log_id[%lu]",
						 fn.c_str(), start_log_id, curWalFile->lastIdx());
				return false;
			}
			curWalFile->setNormal();
	    }

		if (i == (files.size() - 1) ) {
			if (!curWalFile->isLoadFull()) {
				wFd = rfd;
				curWalPos = FileUtils::fileSize(path.c_str());

				walFiles.emplace(sMaxLogIndex, curWalFile);

				// empty
				if (curWalFile->lastIdx() == 0) {
					lastIdx = curWalFile->startIdx() - 1;

					// fprintf(stderr, "last file empty! fn[%s] start_log_id[%lu] startId[%lu] lastIdx[%lu]\n",
					// 		fn.c_str(), start_log_id, curWalFile->startIdx(), lastIdx.load());
				} else {
					// normal
					lastIdx = curWalFile->lastIdx();

					// fprintf(stderr, "last file normal! fn[%s] start_log_id[%lu] startId[%lu] lastIdx[%lu]\n",
					// 		fn.c_str(), start_log_id, curWalFile->startIdx(), lastIdx.load());
				}
			} else {
				wFd = -1;
				curWalPos = 0;

				walFiles.emplace(curWalFile->lastIdx(), curWalFile);

				// full
				lastIdx = curWalFile->lastIdx();

				// fprintf(stderr, "last file full! fn[%s] start_log_id[%lu] startId[%lu] lastIdx[%lu]\n",
				// 			fn.c_str(), start_log_id, curWalFile->startIdx(), lastIdx.load());
			}
		} else {
			if (!curWalFile->isLoadFull()) {
				_log_err(myLog, "isLoadFull failed! fn[%s] start_log_id[%lu] last_log_id[%lu] lastIdx[%lu]",
						fn.c_str(), start_log_id, curWalFile->lastIdx(), lastIdx.load());
				return false;
			}

			walFiles.emplace(curWalFile->lastIdx(), curWalFile);
		}
	}
	return true;
}

// 1. wal files is empty
// 2. wal files 1,2,3, and 3 is partial
// 3. wal files 1,2,3, and 3 is full
// 4. wal files 1,2,3, and 3 is empty
bool WalStore::load()
{
	if (!FileUtils::exist(walDir)) {
		FileUtils::makeDir(walDir, 0644);
	}

	if (!scanAllWalFiles()) return false;

	if (wFd == -1) {
		if (!prepareNewFile(lastIdx + 1)) return false;
	}

	if (startIdx == 0) startIdx.store(1);
	nextIdx = lastIdx + 1;
	return true;
}

void WalStore::rollback(uint64_t last_idx)
{
	std::unique_lock<std::mutex> lk(walFileInfoLock);
	auto iter = walFiles.lower_bound(last_idx);
	if (iter == walFiles.end()) return ;

	uint64_t end_idx = iter->first;
	WalFileInfoPtr info = iter->second;

	iter++;
	while (iter != walFiles.end()) {
		// rename file and then delete
		iter = walFiles.erase(iter);
	}

	uint32_t size = info->rollback(last_idx);

	curWalFile = info;
	curWalPos = size;
	nextIdx = last_idx + 1;

	return ;
}

bool WalStore::initial(const std::string& dir)
{
	if (!walDir.empty()) return false;

	walDir = dir;
	if (!load()) return false;

	return true;
}

std::string WalStore::toString()
{

	return std::string();
}

bool WalStore::writeAt(uint64_t idx, std::shared_ptr<Buffer> data)
{
	Writer w;
	w.data = data;
  	w.sync = false;
  	w.done = false;

	std::unique_lock<std::mutex> l(cvLock);
	if (idx > nextIdx) {
		return false;
	}

	if (idx < nextIdx) {
		// rollback
		rollback(idx - 1);
	}

	w.seq = nextIdx.fetch_add(1);
	writers.push_back(&w);

	while (!w.done && &w != writers.front()) {
		w.cv.wait(l);
	}

	if (w.done) {
		return w.seq;
	}

	Writer* last_writer = &w;
	auto batch = buildBatchGroup(&last_writer);

	cvLock.unlock();

	int64_t bytes_writen = write(wFd, batch->data(), batch->dataLen());
	if (bytes_writen != (int64_t)batch->dataLen()) {
		return 0;
	}

	// TODO:
	// add curWalPos

	// rotate
	if (curWalPos > 16*1024*1024) {
		auto idxbuf = curWalFile->buildIndex();

		uint32_t pos = curWalPos;
		uint32_t length = idxbuf->dataLen();

		auto buf = Buffer::alloc(8);
		buf->writeInt32(pos);
		buf->writeInt32(length);

		char md5[65] = {0};
		MD5AndEncode((uint8_t*)buf->data(), buf->dataLen(), (uint8_t*)md5);

		idxbuf->writeBytes(buf->data(), buf->dataLen());
		idxbuf->writeBytes(md5, sizeof(md5));

		bytes_writen = write(wFd, idxbuf->data(), idxbuf->dataLen());
		if (bytes_writen != (int64_t)idxbuf->dataLen()) {
			// 
		}

		prepareNewFile(last_writer->seq + 1);
	}

	lastIdx.store(last_writer->seq);

	cvLock.lock();

	while (true) {
	    Writer* ready = writers.front();
	    writers.pop_front();
	    if (ready != &w) {
			ready->ret = 0;
			ready->done = true;
			ready->cv.notify_one();
	    }
	    if (ready == last_writer) break;
	}

	// Notify new head of write queue
	if (!writers.empty()) {
		writers.front()->cv.notify_one();
	}

	return true;
}

std::shared_ptr<raftcc::log_entry> WalStore::last_entry() const
{
	Buffer::ptr buf = entryAt(lastIdx.load());
	if (!buf) return nullptr;

	buf->pos(4);
	std::shared_ptr<raftcc::buffer> p = raftcc::buffer::alloc(buf->curDataLen());
	p->writeRaw( (const uint8_t*)buf->curData(), buf->curDataLen() );
	p->pos(0);
	std::shared_ptr<raftcc::log_entry> entry = raftcc::log_entry::deserialize(*p);

    return entry;
}

uint64_t WalStore::append(std::shared_ptr<raftcc::log_entry> entry)
{
	std::shared_ptr<raftcc::buffer> data = entry->serialize();
	std::shared_ptr<Buffer> buf = std::make_shared<Buffer>((uint8_t*)data->data_begin(), data->size());

	return appendLog(buf);
}

void WalStore::write_at(uint64_t index, std::shared_ptr<raftcc::log_entry> entry)
{
	std::shared_ptr<raftcc::buffer> data = entry->serialize();
	std::shared_ptr<Buffer> buf = std::make_shared<Buffer>((uint8_t*)data->data_begin(), data->size());

	writeAt(index, buf);
}

std::shared_ptr<std::vector<std::shared_ptr<raftcc::log_entry>>> WalStore::log_entries(uint64_t start, uint64_t end)
{
	std::vector<Buffer::ptr> vec = logEntries(start, end);
	std::shared_ptr< std::vector< std::shared_ptr<raftcc::log_entry> > > ret =
        std::make_shared< std::vector< std::shared_ptr<raftcc::log_entry> > >();

	for (auto buf : vec) {
		buf->pos(4);
		std::shared_ptr<raftcc::buffer> p = raftcc::buffer::alloc(buf->curDataLen());
		p->writeRaw( (const uint8_t*)buf->curData(), buf->curDataLen() );
		p->pos(0);
		std::shared_ptr<raftcc::log_entry> entry = raftcc::log_entry::deserialize(*p);

	    ret->push_back( entry );
	}

	return ret;
}

std::shared_ptr<std::vector<std::shared_ptr<raftcc::log_entry>>> WalStore::log_entries_ext(
            	uint64_t start, uint64_t end, uint64_t batch_size_hint_in_bytes)
{
	std::vector<Buffer::ptr> vec = logEntries(start, end);
	std::shared_ptr< std::vector< std::shared_ptr<raftcc::log_entry> > > ret =
        std::make_shared< std::vector< std::shared_ptr<raftcc::log_entry> > >();

    uint64_t accum_size = 0;
	for (auto buf : vec) {
		buf->pos(4);
		std::shared_ptr<raftcc::buffer> p = raftcc::buffer::alloc(buf->curDataLen());
		p->writeRaw( (const uint8_t*)buf->curData(), buf->curDataLen() );
		p->pos(0);
		std::shared_ptr<raftcc::log_entry> entry = raftcc::log_entry::deserialize(*p);

	    ret->push_back( entry );

	    accum_size += entry->get_buf().size();
	    if (batch_size_hint_in_bytes && accum_size >= batch_size_hint_in_bytes) break;
	}

	return ret;
}

std::shared_ptr<raftcc::log_entry> WalStore::entry_at(uint64_t index)
{
	Buffer::ptr buf = entryAt(index);
	if (!buf) return nullptr;

	buf->pos(4);
	std::shared_ptr<raftcc::buffer> p = raftcc::buffer::alloc(buf->curDataLen());
	p->writeRaw( (const uint8_t*)buf->curData(), buf->curDataLen() );
	p->pos(0);
	std::shared_ptr<raftcc::log_entry> entry = raftcc::log_entry::deserialize(*p);

    return entry;
}

uint64_t WalStore::term_at(uint64_t index)
{
	Buffer::ptr buf = entryAt(index);
	if (!buf) return 0;

	buf->pos(4);
	std::shared_ptr<raftcc::buffer> p = raftcc::buffer::alloc(buf->curDataLen());
	p->writeRaw( (const uint8_t*)buf->curData(), buf->curDataLen() );
	p->pos(0);

	uint64_t term = p->readInt64();
	return term;
}

std::shared_ptr<raftcc::buffer> WalStore::pack(uint64_t index, int32_t cnt)
{
	return nullptr;
}

void WalStore::apply_pack(uint64_t index, raftcc::buffer& pack)
{
	if (index == 0) return ;

	rollback(index - 1);

	std::shared_ptr<Buffer> buf = std::make_shared<Buffer>((uint8_t*)pack.data_begin(), pack.size());

	writeAt(index, buf);
}

bool WalStore::compact(uint64_t last_log_index)
{
	return true;
}

bool WalStore::flush()
{
	return true;
}

uint64_t WalStore::appendLog(std::shared_ptr<Buffer> data)
{
	Writer w;
	w.data = data;
  	w.sync = false;
  	w.done = false;

	std::unique_lock<std::mutex> l(cvLock);
	w.seq = nextIdx.fetch_add(1);
	writers.push_back(&w);

	while (!w.done && &w != writers.front()) {
		w.cv.wait(l);
	}

	if (w.done) {
		return w.seq;
	}

	Writer* last_writer = &w;
	auto batch = buildBatchGroup(&last_writer);

	cvLock.unlock();

	int64_t bytes_writen = write(wFd, batch->data(), batch->dataLen());

	// _log_info(myLog, "bytes_writen: %d expected_len: %d", bytes_writen, batch->dataLen());
	if (bytes_writen != (int64_t)batch->dataLen()) {
		return 0;
	}

	// rotate
	if (curWalPos > 16*1024*1024) {
		auto idxbuf = curWalFile->buildIndex();

		uint32_t pos = curWalPos;
		uint32_t length = idxbuf->dataLen();

		auto buf = Buffer::alloc(8);
		buf->writeInt32(pos);
		buf->writeInt32(length);

		char md5[65] = {0};
		MD5AndEncode((uint8_t*)buf->data(), buf->dataLen(), (uint8_t*)md5);

		idxbuf->writeBytes(buf->data(), buf->dataLen());
		idxbuf->writeBytes(md5, sizeof(md5));

		bytes_writen = write(wFd, idxbuf->data(), idxbuf->dataLen());
		if (bytes_writen != (int64_t)idxbuf->dataLen()) {
			// 
		}

		prepareNewFile(last_writer->seq + 1);
	}

	lastIdx.store(last_writer->seq);

	cvLock.lock();

	while (true) {
	    Writer* ready = writers.front();
	    writers.pop_front();
	    if (ready != &w) {
			ready->ret = 0;
			ready->done = true;
			ready->cv.notify_one();
	    }
	    if (ready == last_writer) break;
	}

	// Notify new head of write queue
	if (!writers.empty()) {
		writers.front()->cv.notify_one();
	}

	return w.seq;
}

std::shared_ptr<Buffer> WalStore::entryAt(const uint64_t idx) const
{
	if (idx < startIdx.load() || idx > lastIdx.load()) {
		_log_err(myLog, "idx: %lu startIdx: %lu lastIdx: %lu", idx, startIdx.load(), lastIdx.load());
		return nullptr;
	}

	WalFileInfoPtr info = nullptr;
	uint64_t end_idx = 0;
	{
		std::unique_lock<std::mutex> lk(walFileInfoLock);
		auto iter = walFiles.lower_bound(idx);
		if (iter == walFiles.end()) return nullptr;

		end_idx = iter->first;
		info = iter->second;
	}

	uint64_t ivt = info->getIvtIndex(idx);
	if (ivt == 0) {
		_log_err(myLog, "idx: %lu startIdx: %lu lastIdx: %lu", idx, startIdx.load(), lastIdx.load());
		return nullptr;
	}

	uint32_t pos = (ivt >> 32) & (uint32_t)0xFFFFFFFF;
	uint32_t length = ((uint32_t)(ivt) & (uint32_t)0xFFFFFFFF);

	std::shared_ptr<Buffer> buf = Buffer::alloc(length);
	int read_len = pread(info->getFd(), buf->data(), length, pos);
	if ( read_len != length) {
		_log_err(myLog, "idx: %lu startIdx: %lu lastIdx: %lu pos: %lu length: %u read_len: %d ivt: %lu",
				 idx, startIdx.load(), lastIdx.load(), pos, length, read_len, ivt);
      	return nullptr;
    }

    buf->pourData(length);
    return buf;
}

std::vector<std::shared_ptr<Buffer>> WalStore::logEntries(uint64_t sidx, uint64_t eidx)
{
	std::vector<std::shared_ptr<Buffer>> vecs;

	if ( (sidx >= eidx) || (sidx < startIdx.load()) || (sidx > lastIdx.load()) || 
		 (eidx < startIdx.load()) ) {
		// fprintf(stderr, "sidx: %lu eidx: %lu startIdx: %lu lastIdx: %lu\n",
		// 		sidx, eidx, startIdx.load(), lastIdx.load());
		_log_err(myLog, "sidx: %lu eidx: %lu startIdx: %lu lastIdx: %lu",
		 		sidx, eidx, startIdx.load(), lastIdx.load());
		return vecs;
	}

	WalFileInfoPtr info = nullptr;
	uint64_t end_idx = 0;
	{
		std::unique_lock<std::mutex> lk(walFileInfoLock);
		auto iter = walFiles.lower_bound(sidx);
		if (iter == walFiles.end()) {
			std::string str;
			for (auto ele : walFiles) {
				str += " " + std::to_string(ele.first);
			}

			_log_err(myLog, "sidx: %lu str: %s", sidx, str.c_str());
			return vecs;
		}

		end_idx = iter->first;
		info = iter->second;

		// fix issue
		if (sidx == end_idx) {
			end_idx += 1;
		}

		if (end_idx > eidx) {
			end_idx = eidx;
		}
	}

	uint32_t start_pos = 0xFFFFFFFF;
	uint32_t total_length = 0;
	std::vector<uint32_t> lengths;
	for (uint64_t idx = sidx; idx < end_idx; idx++) {
		uint64_t ivt = info->getIvtIndex(idx);
		if (ivt == 0) {
			_log_err(myLog, "idx: %lu startIdx: %lu lastIdx: %lu", idx, startIdx.load(), lastIdx.load());
			// return vecs;
			break;
		}

		uint32_t pos = (ivt >> 32) & (uint32_t)0xFFFFFFFF;
		uint32_t length = ((uint32_t)(ivt) & (uint32_t)0xFFFFFFFF);

		// TODO
		if (start_pos == 0xFFFFFFFF) start_pos = pos;

		lengths.push_back(length);
		total_length += length;
	}

	std::shared_ptr<Buffer> buf = Buffer::alloc(total_length);
	int read_len = pread(info->getFd(), buf->data(), total_length, start_pos);
	if ( read_len != total_length) {
		_log_err(myLog, "idx: %lu startIdx: %lu lastIdx: %lu pos: %lu length: %u read_len: %d",
				sidx, startIdx.load(), lastIdx.load(), start_pos, total_length, read_len);
      	return vecs;
    }
    buf->pourData(total_length);

    for (auto& len : lengths) {
    	std::shared_ptr<Buffer> tmpbuf = Buffer::alloc(len);
    	buf->readBytes(tmpbuf->data(), len);
    	tmpbuf->pourData(len);

    	vecs.push_back(tmpbuf);
    }
    // _log_warn(myLog, "vecs.size() = %d, %lu - %lu %lu", vecs.size(), sidx, end_idx, eidx);
	return vecs;
}

std::shared_ptr<Buffer> WalStore::buildBatchGroup(Writer** last_writer) {
  	Writer* first = writers.front();
  	std::shared_ptr<Buffer> rep = Buffer::alloc(1024);

  	*last_writer = first;
  	std::deque<Writer*>::iterator iter = writers.begin();
	for (; iter != writers.end(); ++iter) {
		Writer* w = *iter;
		if (w->sync && !first->sync) {
			break;
		}

		rep->writeInt32(w->data->dataLen());
		rep->writeBytes(w->data->data(), w->data->dataLen());

		uint64_t size = ((uint64_t)curWalPos << 32) | (uint64_t)(sizeof(uint32_t) + w->data->dataLen());
		curWalFile->insert(w->seq, size);
		// fprintf(stderr, "seq: %lu curWalPos: %u length: %u size: %lu\n", w->seq, curWalPos, w->data->dataLen(), size);

		curWalPos += (sizeof(int32_t) + w->data->dataLen()); 
		*last_writer = w;
	}
	return rep;
}

bool WalStore::prepareNewFile(uint64_t start_log_id)
{
	char path[2048] = {0};
  	snprintf(path, 2048, "%s/%019ld.wal", walDir.c_str(), (int64_t)start_log_id);

  	curWalPos = 0;

  	WalFileInfoPtr info = std::make_shared<WalFileInfo>(path, start_log_id);
  	info->setNormal();
  	wFd = info->getFd();

  	{
  		std::unique_lock<std::mutex> lk(walFileInfoLock);
  		auto iter = walFiles.find(sMaxLogIndex);
  		if (iter != walFiles.end()) {
  			if (start_log_id != 1) {
  				walFiles.emplace(start_log_id - 1, iter->second);
  			}

  			walFiles.erase(iter);
  		}

  		walFiles.emplace(sMaxLogIndex, info);
  	}

  	curWalFile = info;

  	return true;
}

}
