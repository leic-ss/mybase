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

#include "kv_log_store.h"
#include "buffer_helper.h"

#include "libnuraft/nuraft.hxx"
#include "libnuraft/tracer.hxx"

#include <cassert>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>

namespace mybase {

KvLogStore::KvLogStore(const std::string& log_dir, ptr<logger> logger_)
    : dbPath(log_dir)
    , sysFilePath(log_dir + "/sys_log_store")
    , l_(logger_)
{
    assert(open_database());
    assert(open_sys_file());
}

bool KvLogStore::open_sys_file()
{
    int32_t fd = open(sysFilePath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        p_er("open sys file failed! file[%s]", sysFilePath.c_str());
        return false;
    }
    ftruncate(fd, sysFileSize);

    sysPtr = (uint8_t*)mmap(0, sysFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sysPtr == MAP_FAILED) {
        p_er("mmap sys file failed! file[%s] err[%s]", sysFilePath.c_str(), strerror(errno));
        return false;
    }

    ::close(fd);
    startIdxPtr = (uint64_t*)sysPtr;
    nextIdxPtr = (uint64_t*)(sysPtr + sizeof(uint64_t));

    if (*startIdxPtr == 0) *startIdxPtr = 1;
    if (*nextIdxPtr == 0) *nextIdxPtr = 1;

    return true;
}

KvLogStore::~KvLogStore()
{
    for (auto handle : cfHandles) {
        p_in("delete handle success!");
        delete handle;
    }

    if ( db ) {
        delete db;
        p_in("delete data db success!");
        db = nullptr;
    }

    if ( (sysPtr != MAP_FAILED) && (munmap(sysPtr, sysFileSize)) < 0) {
        p_er("munmap sys file failed! file[%s] err[%s]", sysFilePath.c_str(), strerror(errno));
    }
}

ptr<log_entry> KvLogStore::make_clone(const ptr<log_entry>& entry)
{
    ptr<log_entry> clone = cs_new<log_entry> ( entry->get_term(),
                                               buffer::clone( entry->get_buf() ),
                                               entry->get_val_type() );
    return clone;
}

uint64_t KvLogStore::next_slot() const
{
    std::lock_guard<std::mutex> l(logs_lock_);
    return *nextIdxPtr;
}

uint64_t KvLogStore::start_index() const
{
    return *startIdxPtr;
}

ptr<log_entry> KvLogStore::last_entry() const
{
    std::lock_guard<std::mutex> l(logs_lock_);

    if (*startIdxPtr == *nextIdxPtr) {
        return ptr<log_entry>();
    }

    uint64_t idx = *nextIdxPtr - 1;
    char buf[8] = {0};
    put64b(idx, buf);

    std::string key((const char*)buf, 8);
    std::string value;
    rocksdb::Status s = db->Get(rocksdb::ReadOptions(), cfHandles[0], key, &value);
    if (!s.ok()) {
        p_er("get failed! key[%lu] err[%s]", idx, s.ToString().c_str());
        return nullptr;
    }

    ptr<buffer> data = buffer::alloc( value.size() );
    data->put_raw((const uint8_t*)value.data(), value.size());

    data->pos(0);
    ptr<log_entry> entry = log_entry::deserialize(*data);
    return entry;
}

uint64_t KvLogStore::append(ptr<log_entry>& entry)
{
    ptr<log_entry> clone = make_clone(entry);

    std::lock_guard<std::mutex> l(logs_lock_);
    char buf[8] = {0};
    put64b(*nextIdxPtr, buf);

    std::string key((const char*)buf, 8);
    ptr<buffer> data = clone->serialize();
    std::string value((const char*)data->data_begin(), data->size());

    rocksdb::Status s = db->Put(rocksdb::WriteOptions(), cfHandles[0], key, value);
    if (!s.ok()) {
        p_er("put failed! key[%lu] sidx[%lu] err[%s]", *nextIdxPtr, *startIdxPtr, s.ToString().c_str());
        return 0;
    }

    return (*nextIdxPtr)++;
}

void KvLogStore::write_at(uint64_t index, ptr<log_entry>& entry)
{
    // Discard all logs equal to or greater than `index.
    ptr<log_entry> clone = make_clone(entry);

    std::lock_guard<std::mutex> l(logs_lock_);
    char buf[8] = {0};
    put64b(index, buf);

    std::string key((const char*)buf, 8);
    ptr<buffer> data = clone->serialize();
    std::string value((const char*)data->data_begin(), data->size());

    rocksdb::Status s = db->Put(rocksdb::WriteOptions(), cfHandles[0], key, value);
    if (!s.ok()) {
        p_er("put failed! key[%lu] err[%s]", index, s.ToString().c_str());
        return ;
    }

    return ;
}

ptr< std::vector< ptr<log_entry> > > KvLogStore::log_entries(uint64_t start, uint64_t end)
{
    ptr< std::vector< ptr<log_entry> > > ret = cs_new< std::vector< ptr<log_entry> > >();

    ret->resize(end - start);

    char sbuf[8] = {0};
    put64b(start, sbuf);
    rocksdb::Slice startKey((const char*)sbuf, 8);

    char ebuf[8] = {0};
    put64b(end, ebuf);
    rocksdb::Slice endKey((const char*)ebuf, 8);

    rocksdb::ReadOptions scan_read_options;
    scan_read_options.fill_cache = false; // not fill cache
    scan_read_options.iterate_lower_bound = &startKey;
    scan_read_options.iterate_upper_bound = &endKey;

    rocksdb::Iterator* scan_it_ = db->NewIterator(scan_read_options);
    if (!scan_it_) return ret;

    scan_it_->SeekToFirst();
    while(scan_it_->Valid()) {
        ptr<buffer> data = buffer::alloc( scan_it_->value().size() );
        data->put_raw((const uint8_t*)scan_it_->value().data(), scan_it_->value().size());

        data->pos(0);
        ptr<log_entry> entry = log_entry::deserialize(*data);
        ret->push_back(entry);

        scan_it_->Next();
    }
    delete scan_it_;

    return ret;
}

ptr<std::vector<ptr<log_entry>>> KvLogStore::log_entries_ext(uint64_t start,
                                     uint64_t end, uint64_t batch_size_hint_in_bytes)
{
    ptr< std::vector< ptr<log_entry> > > ret = cs_new< std::vector< ptr<log_entry> > >();

    char sbuf[8] = {0};
    put64b(start, sbuf);
    rocksdb::Slice startKey((const char*)sbuf, 8);

    char ebuf[8] = {0};
    put64b(end, ebuf);
    rocksdb::Slice endKey((const char*)ebuf, 8);

    rocksdb::ReadOptions scan_read_options;
    scan_read_options.fill_cache = false; // not fill cache
    scan_read_options.iterate_lower_bound = &startKey;
    scan_read_options.iterate_upper_bound = &endKey;

    rocksdb::Iterator* scan_it_ = db->NewIterator(scan_read_options);
    if (!scan_it_) return ret;

    uint64_t accum_size = 0;

    scan_it_->SeekToFirst();
    while(scan_it_->Valid()) {
        ptr<buffer> data = buffer::alloc( scan_it_->value().size() );
        data->put_raw((const uint8_t*)scan_it_->value().data(), scan_it_->value().size());

        data->pos(0);
        ptr<log_entry> entry = log_entry::deserialize(*data);
        ret->push_back(entry);

        accum_size += entry->get_buf().size();
        if (batch_size_hint_in_bytes && accum_size >= batch_size_hint_in_bytes) break;

        scan_it_->Next();
    }
    delete scan_it_;

    return ret;
}

ptr<log_entry> KvLogStore::entry_at(uint64_t index)
{
    std::lock_guard<std::mutex> l(logs_lock_);
    char sbuf[8] = {0};
    put64b(index, sbuf);

    std::string key((const char*)sbuf, 8);
    std::string value;
    rocksdb::Status s = db->Get(rocksdb::ReadOptions(), key, &value);
    if (!s.ok()) {
        p_er("get failed! key[%lu] err[%s]", index, s.ToString().c_str());
        return nullptr;
    }

    ptr<buffer> data = buffer::alloc( value.size() );
    data->put_raw((const uint8_t*)value.data(), value.size());

    data->pos(0);
    ptr<log_entry> entry = log_entry::deserialize(*data);
    return entry;
}

uint64_t KvLogStore::term_at(uint64_t index)
{
    std::lock_guard<std::mutex> l(logs_lock_);
    char sbuf[8] = {0};
    put64b(index, sbuf);

    std::string key((const char*)sbuf, 8);
    std::string value;
    rocksdb::Status s = db->Get(rocksdb::ReadOptions(), key, &value);
    if (!s.ok()) {
        p_er("get failed! key[%lu] err[%s]", index, s.ToString().c_str());
        return 0;
    }

    ptr<buffer> data = buffer::alloc( value.size() );
    data->put_raw((const uint8_t*)value.data(), value.size());

    data->pos(0);
    uint64_t term = data->get_ulong();
    return term;
}

ptr<buffer> KvLogStore::pack(uint64_t index, int32_t cnt)
{
    std::vector< ptr<buffer> > logs;

    size_t size_total = 0;
    auto log_batch = log_entries(index, index + cnt);
    if (!log_batch) return nullptr;

    for (int32_t ii=0; ii<cnt; ++ii) {
        ptr<log_entry> le = (*log_batch)[ii];
        assert(le.get());

        ptr<buffer> buf = le->serialize();
        size_total += buf->size();
        logs.push_back( buf );
    }

    ptr<buffer> buf_out = buffer::alloc( sizeof(int32_t) + cnt * sizeof(int32_t) + size_total );
    buf_out->pos(0);
    buf_out->put((int32_t)cnt);

    for (auto& entry: logs) {
        ptr<buffer>& bb = entry;
        buf_out->put((int32_t)bb->size());
        buf_out->put(*bb);
    }
    buf_out->pos(0);
    return buf_out;
}

void KvLogStore::apply_pack(uint64_t index, buffer& pack)
{
    pack.pos(0);
    int32_t num_logs = pack.get_int();

    if (*startIdxPtr == *nextIdxPtr) {
        return ;
    }

    if (index < *startIdxPtr || index > *nextIdxPtr) {
        return ;
    }

    std::lock_guard<std::mutex> l(logs_lock_);

    for (int32_t ii=0; ii<num_logs; ++ii) {
        int32_t buf_size = pack.get_int();

        ptr<buffer> buf_local = buffer::alloc(buf_size);
        pack.get(buf_local);

        ptr<log_entry> le = log_entry::deserialize(*buf_local);
        write_at(index + ii, le);
    }

    if ( (index + num_logs) > *nextIdxPtr) *nextIdxPtr = index + num_logs;
    return ;
}

bool KvLogStore::open_database()
{
    rocksdb::Status s = rocksdb::LoadLatestOptions(dbPath, rocksdb::Env::Default(), &dbOpts, &cfDescs, false);
    if (s.ok()) {
        p_wn("load data db option success! data db path[%s]", dbPath.c_str());
    } else if (s.IsNotFound()) {
        p_wn("data db option not found, create one! data db path[%s]", dbPath.c_str());

        dbOpts.create_if_missing = true;
        dbOpts.create_missing_column_families = true;

        rocksdb::ColumnFamilyOptions cfOpts;
        // cfOpts.OptimizeForSmallDb();
        cfDescs.emplace_back(rocksdb::ColumnFamilyDescriptor(rocksdb::kDefaultColumnFamilyName, cfOpts));
    } else {
        p_er("load data db option failed! data path[%s] err[%s]", dbPath.c_str(), s.ToString().c_str());
        return false;
    }

    if (cfDescs.empty()) {
        p_er("initialize data db failed! data path[%s] err[%s]", dbPath.c_str(), s.ToString().c_str());
        return false;
    }

    s = rocksdb::DB::Open(dbOpts, dbPath, cfDescs, &cfHandles, &db);
    if (!s.ok()) {
        p_er("open data db failed! data db path[%s] err[%s]", dbPath.c_str(), s.ToString().c_str());
        return false;
    }

    for (auto& desc : cfDescs) {
        p_wn("ColumnFamilyDescriptor: %s", desc.name.c_str());
    }
    for (auto handler : cfHandles) {
        p_wn("ColumnFamilyHandle: %s", handler->GetName().c_str());
    }

    p_wn("open data db success! data db path[%s]", dbPath.c_str());
    return true;
}

bool KvLogStore::compact(uint64_t last_log_index)
{
    std::lock_guard<std::mutex> l(logs_lock_);
    for (uint64_t ii = *startIdxPtr; ii <= last_log_index; ++ii) {
        char sbuf[8] = {0};
        put64b(ii, sbuf);

        std::string key((const char*)sbuf, 8);
        std::string value;
        rocksdb::Status s = db->Delete(rocksdb::WriteOptions(), key);
        if (!s.ok()) {
            p_er("delete failed! key[%lu] err[%s]", ii, s.ToString().c_str());
        }
    }

    p_wn("compact! delete sequence[%lu - %lu]", *startIdxPtr, last_log_index);

    *startIdxPtr = last_log_index + 1;
    if (last_log_index >= (*nextIdxPtr - 1)) *nextIdxPtr = last_log_index + 1;
    return true;
}

void KvLogStore::close() {}

}

