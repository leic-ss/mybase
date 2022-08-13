#include "wal.h"
#include "public/md5_encode.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

namespace mybase {

namespace wal {

static uint64_t sMaxLogIndex = 0xFFFFFFFFFFFFFFFF;

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

	for (uint64_t i = idx + 1; i < lastIndex; i++) {
		ivts.erase(i);
	}

	auto iter = ivts.find(idx);
	if (iter == ivts.end()) return 0;

	uint64_t ivt = iter->second;
	uint32_t pos = (ivt >> 32) & (uint32_t)0xFFFFFFFF;
	uint32_t length = ((uint32_t)(ivt) & (uint32_t)0xFFFFFFFF);

	return (pos + length);
}

void WalFileInfo::clean()
{
	rwlock.writeLock();

	ivts.clear();
	setClean();

	rwlock.writeUnlock();
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

Wal::Wal()
{ }

Wal::~Wal()
{ }

bool SortByString( const std::string& v1, const std::string& v2)
{  
    return v1 < v2;
}

bool Wal::scanAllWalFiles()
{
	std::vector<std::string> files = FileUtils::listAllFilesInDir(walDir.c_str(), false, "*.wal");
	std::sort(files.begin(), files.end(), SortByString);

	for (uint32_t i = 0; i < files.size(); i++) {
		std::string fn = files[i];
		std::vector<std::string> parts = StringHelper::tokenize(fn, ".");
	    if (parts.size() != 2) {
		    continue;
	    }

	    int64_t start_log_id = atol(parts[0].c_str());
	    if (start_log_id == 0) continue;

	    if (startIdx == 0) startIdx = start_log_id;

	    curWalFile.reset();
	    std::string path = FileUtils::joinPath(walDir, fn);

	    int32_t rFd = open(path.c_str(), O_RDWR | O_CREAT | O_APPEND | O_LARGEFILE, 0644);
	  	if (rFd < 0) {
	  		continue;
	  	}

	    curWalFile = std::make_shared<WalFileInfo>(path, rFd, start_log_id);

	    // TODO: skip loading with some conditions
	    {
	    	bool rc = curWalFile->loadInfo();
			if (!rc) {
				fprintf(stderr, "load info failed! fn[%s] start_log_id[%lu] last_log_id[%lu]\n",
						fn.c_str(), start_log_id, curWalFile->lastIdx());
				return false;
			}
			curWalFile->setNormal();
	    }

		if (i == (files.size() - 1) ) {
			if (!curWalFile->isLoadFull()) {
				wFd = rFd;
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
				fprintf(stderr, "isLoadFull failed! fn[%s] start_log_id[%lu] last_log_id[%lu] lastIdx[%lu]\n",
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
bool Wal::load()
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

void Wal::rollback(uint64_t last_idx)
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

bool Wal::initial(const std::string& dir)
{
	if (!walDir.empty()) return false;

	walDir = dir;
	if (!load()) return false;

	return true;
}

std::string Wal::toString()
{

	return std::string();
}

bool Wal::writeAt(uint64_t idx, std::shared_ptr<Buffer> data)
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

uint64_t Wal::appendLog(std::shared_ptr<Buffer> data)
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

	return 0;
}

std::shared_ptr<Buffer> Wal::entryAt(uint64_t idx)
{
	if (idx < startIdx.load() || idx > lastIdx.load()) {
		fprintf(stderr, "idx: %lu startIdx: %lu lastIdx: %lu\n", idx, startIdx.load(), lastIdx.load());
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
		fprintf(stderr, "2 idx: %lu startIdx: %lu lastIdx: %lu\n", idx, startIdx.load(), lastIdx.load());
		return nullptr;
	}

	uint32_t pos = (ivt >> 32) & (uint32_t)0xFFFFFFFF;
	uint32_t length = ((uint32_t)(ivt) & (uint32_t)0xFFFFFFFF);

	std::shared_ptr<Buffer> buf = Buffer::alloc(length);
	int read_len = pread(info->getFd(), buf->data(), length, pos);
	if ( read_len != length) {
		fprintf(stderr, "idx: %lu startIdx: %lu lastIdx: %lu pos: %lu length: %u read_len: %d ivt: %lu\n",
				idx, startIdx.load(), lastIdx.load(), pos, length, read_len, ivt);
      	return nullptr;
    }

    buf->pourData(length);
    return buf;
}

std::vector<std::shared_ptr<Buffer>> Wal::logEntries(uint64_t sidx, uint64_t eidx)
{
	std::vector<std::shared_ptr<Buffer>> vecs;

	if ( (sidx > eidx) || (sidx < startIdx.load()) || (sidx > lastIdx.load()) || 
		 (eidx < startIdx.load()) || (eidx > lastIdx.load()) ) {
		// fprintf(stderr, "sidx: %lu eidx: %lu startIdx: %lu lastIdx: %lu\n",
		// 		sidx, eidx, startIdx.load(), lastIdx.load());
		return vecs;
	}

	WalFileInfoPtr info = nullptr;
	uint64_t end_idx = 0;
	{
		std::unique_lock<std::mutex> lk(walFileInfoLock);
		auto iter = walFiles.lower_bound(sidx);
		if (iter == walFiles.end()) return vecs;

		end_idx = iter->first;
		info = iter->second;

		if (end_idx > eidx) {
			end_idx = eidx;
		}
	}

	uint32_t start_pos = 0xFFFFFFFF;
	uint32_t total_length = 0;
	std::vector<uint32_t> lengths;
	for (uint64_t idx = sidx; idx <= end_idx; idx++) {
		uint64_t ivt = info->getIvtIndex(idx);
		if (ivt == 0) {
			fprintf(stderr, "2 idx: %lu startIdx: %lu lastIdx: %lu\n", idx, startIdx.load(), lastIdx.load());
			return vecs;
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
		fprintf(stderr, "idx: %lu startIdx: %lu lastIdx: %lu pos: %lu length: %u read_len: %d\n",
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
	return vecs;
}

std::shared_ptr<Buffer> Wal::buildBatchGroup(Writer** last_writer) {
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

bool Wal::prepareNewFile(uint64_t start_log_id)
{
	char path[2048] = {0};
  	snprintf(path, 2048, "%s/%019ld.wal", walDir.c_str(), (int64_t)start_log_id);

	wFd = open(path, O_RDWR | O_CREAT | O_APPEND | O_LARGEFILE, 0644);
  	if (wFd < 0) {
  		fprintf(stderr, "open failed! path[%s] start_log_id[%lu]\n", path, start_log_id);
  		return false;
  	}

  	curWalPos = 0;

  	WalFileInfoPtr info = std::make_shared<WalFileInfo>(path, wFd, start_log_id);
  	info->setNormal();

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

}
