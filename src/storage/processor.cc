#include "storage.h"

namespace mybase
{

void StorageServer::process(KvRequestPut* msg, KvResponseReturn* resp)
{
	if (!msg || !resp) {
		resp->set_code(OP_RETURN_FAILED);
		return ;
	}

	uint32_t bucket_no = getBucketNumber(msg->key());
	int32_t rc = kvengine->put(msg->ns(), bucket_no, msg->key(), msg->val(), 0, msg->expired());

	resp->set_code(rc);
    resp->set_metaversion(metaVersion);
    return ;
}

void StorageServer::process(KvRequestGet* msg, KvResponseGet* resp)
{
	if (!msg || !resp) {
		resp->set_code(OP_RETURN_FAILED);
		return ;
	}

	uint32_t bucket_no = getBucketNumber(msg->key());

	auto kv = resp->mutable_kv();
	int32_t rc = kvengine->get(msg->ns(), bucket_no, msg->key(), *kv);

	resp->set_code(rc);
    resp->set_metaversion(metaVersion);
    return ;
}

void StorageServer::commit(Buffer::ptr data)
{
    uint32_t log_size = data->readInt32();
    _log_info(myLog, "size: %d logsize: %d", data->dataLen(), log_size);

    data->pos( sizeof(uint32_t) );
    int32_t sock_index = data->readInt32();
    uint32_t sock_create = data->readInt32();

    data->pos( 5 * sizeof(uint32_t) + sizeof(uint16_t) );
    DumpEntry entry;
    if ( !entry.ParseFromArray(data->curData(), data->curDataLen()) ) {
        _log_err(myLog, "parse failed!");
        return ;
    }

    auto rsp_msg = std::make_shared<KvResponseReturn>();
    KvResponseReturn* resp = _RC(KvResponseReturn*, rsp_msg.get());

    uint32_t bucket_no = getBucketNumber(entry.key());
    int32_t rc = kvengine->put(entry.ns(), bucket_no, entry.key(), entry.val(), 0, entry.expired());

    resp->set_code(rc);
    resp->set_metaversion(metaVersion);

    _log_info(myLog, "commit entry %s", entry.DebugString().c_str());

    do {
        if (!rsp_msg) {
            break;
        }

        std::string value;
        rsp_msg->SerializeToString(&value);

        auto buff = Buffer::alloc(1024);
        buff->writeInt32(0);
        buff->writeInt32(entry.channelid());
        buff->writeInt32(KV_RES_MESSAGE_RETURN);
        buff->writeBytes(value.data(), value.size());
        buff->fillInt32((uint8_t*)buff->data(), buff->dataLen());

        sendResponse(sock_index, sock_create, buff->data(), buff->dataLen());
    } while(false);
}

}
