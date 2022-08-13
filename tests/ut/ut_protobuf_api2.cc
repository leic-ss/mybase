#include "unit_test.pb.h"
#include "demo.pb.h"
#include "common.h"

#include "gtest/gtest.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <assert.h>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

using namespace google::protobuf;
using namespace google::protobuf::compiler;
using namespace google::protobuf::io;
using namespace google::protobuf::internal;

static std::unordered_map<int32_t, std::vector<UnknownField*>> parse_unknow_field_sets(UnknownFieldSet* sets)
{
	std::unordered_map<int32_t, std::vector<UnknownField*>> child_fields;

	if (!sets) return std::move(child_fields);

	uint32_t field_count = sets->field_count();
	for (uint32_t i = 0; i < field_count; i++) {
        UnknownField* fid = sets->mutable_field(i);
        
        int32_t id = fid->number();
        if (child_fields.find(id) == child_fields.end()) {
        	child_fields.emplace(id, std::vector<UnknownField*>());	
        }

        auto iter = child_fields.find(id);
        auto& vec = iter->second;
        vec.push_back(fid);
    }

    return std::move(child_fields);
}

// TODO: return it if null
static bool pb_update_int_raw(std::shared_ptr<Message> msg, uint32_t fid, int64_t val, bool force = false)
{
    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(sets);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (force) {
            sets->AddVarint(fid, val);
            return true;
        }
        return false;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != UnknownField::Type::TYPE_VARINT) {
        return false;
    }

    field->set_varint(val);
    return true;
}

static bool pb_update_int_raw2(UnknownFieldSet& fid_set, uint32_t fid, int64_t val, bool force = false)
{
    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(&fid_set);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (force) {
            fid_set.AddVarint(fid, val);
            return true;
        }
        return false;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != UnknownField::Type::TYPE_VARINT) {
        return false;
    }

    field->set_varint(val);
    return true;
}

static bool pb_incr_int_raw(std::shared_ptr<Message> msg, uint32_t fid, int64_t val, bool force = false)
{
    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(sets);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (force) {
            sets->AddVarint(fid, val);
            return true;
        }
        return false;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != UnknownField::Type::TYPE_VARINT) {
        return false;
    }

    uint64_t value = field->varint();
    value += val;
    field->set_varint(value);
    return true;
}

static bool pb_update_fixed_raw(std::shared_ptr<Message> msg, uint32_t fid, int64_t val, enum UnknownField::Type type, bool force = false)
{
    if (type != UnknownField::Type::TYPE_FIXED32 && type != UnknownField::Type::TYPE_FIXED64) {
        return false;
    }

    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(sets);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (!force) {
            return false;
        }

        if (type == UnknownField::Type::TYPE_FIXED32) {
            sets->AddFixed32(fid, val);
        } else if (type == UnknownField::Type::TYPE_FIXED64) {
            sets->AddFixed64(fid, val);
        }

        return true;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != type) {
        return false;
    }

    if (type == UnknownField::Type::TYPE_FIXED32) {
        field->set_fixed32(val);
    } else if (type == UnknownField::Type::TYPE_FIXED64) {
        field->set_fixed64(val);
    }

    return true;
}

static bool pb_incr_fixed_raw(std::shared_ptr<Message> msg, uint32_t fid, int64_t val, enum UnknownField::Type type, bool force = false)
{
    if (type != UnknownField::Type::TYPE_FIXED32 && type != UnknownField::Type::TYPE_FIXED64) {
        return false;
    }

    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(sets);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (!force) {
            return false;
        }

        if (type == UnknownField::Type::TYPE_FIXED32) {
            sets->AddFixed32(fid, val);
        } else if (type == UnknownField::Type::TYPE_FIXED64) {
            sets->AddFixed64(fid, val);
        }

        return true;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != type) {
        return false;
    }

    if (type == UnknownField::Type::TYPE_FIXED32) {
        uint32_t value = field->fixed32() + val;
        field->set_fixed32(value);
    } else if (type == UnknownField::Type::TYPE_FIXED64) {
        uint64_t value = field->fixed64() + val;
        field->set_fixed64(value);
    }

    return true;
}

static bool pb_incr_float_raw(std::shared_ptr<Message> msg, uint32_t fid, int64_t val, enum UnknownField::Type type, bool force = false)
{
    if (type != UnknownField::Type::TYPE_FIXED32 && type != UnknownField::Type::TYPE_FIXED64) {
        return false;
    }

    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(sets);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (!force) {
            return false;
        }

        if (type == UnknownField::Type::TYPE_FIXED32) {
            sets->AddFixed32(fid, val);
        } else if (type == UnknownField::Type::TYPE_FIXED64) {
            sets->AddFixed64(fid, val);
        }

        return true;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != type) {
        return false;
    }

    if (type == UnknownField::Type::TYPE_FIXED32) {
        float value = WireFormatLite::DecodeFloat(field->fixed32()) + WireFormatLite::DecodeFloat(val);
        field->set_fixed32(WireFormatLite::EncodeFloat(value));
    } else if (type == UnknownField::Type::TYPE_FIXED64) {
        double value = WireFormatLite::DecodeDouble(field->fixed64()) + WireFormatLite::DecodeDouble(val);
        field->set_fixed64(WireFormatLite::EncodeDouble(value));
    }

    return true;
}

static bool pb_update_int(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    return pb_update_int_raw(msg, fid, val);
}

static bool pb_update_sint(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    uint64_t encoded_val = WireFormatLite::ZigZagEncode64(val);
    return pb_update_int_raw(msg, fid, encoded_val);
}

static bool pb_update_bool(std::shared_ptr<Message> msg, uint32_t fid, bool val)
{
    return pb_update_int_raw(msg, fid, val, true);
}

static bool pb_update_fixed32(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    return pb_update_fixed_raw(msg, fid, val, UnknownField::Type::TYPE_FIXED32);
}

static bool pb_update_fixed64(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    return pb_update_fixed_raw(msg, fid, val, UnknownField::Type::TYPE_FIXED64);
}

static bool pb_update_float(std::shared_ptr<Message> msg, uint32_t fid, float val)
{
    uint32_t encoded_val = WireFormatLite::EncodeFloat(val);
    return pb_update_fixed32(msg, fid, encoded_val);
}

static bool pb_update_double(std::shared_ptr<Message> msg, uint32_t fid, double val)
{
    uint64_t encoded_val = WireFormatLite::EncodeDouble(val);
    return pb_update_fixed64(msg, fid, encoded_val);
}

static bool pb_incr_fixed32(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    return pb_incr_fixed_raw(msg, fid, val, UnknownField::Type::TYPE_FIXED32);
}

static bool pb_incr_fixed64(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    return pb_incr_fixed_raw(msg, fid, val, UnknownField::Type::TYPE_FIXED64);
}

static bool pb_incr_float(std::shared_ptr<Message> msg, uint32_t fid, float val)
{
    uint32_t encoded_val = WireFormatLite::EncodeFloat(val);
    return pb_incr_float_raw(msg, fid, encoded_val, UnknownField::Type::TYPE_FIXED32);
}

static bool pb_incr_double(std::shared_ptr<Message> msg, uint32_t fid, double val)
{
    uint64_t encoded_val = WireFormatLite::EncodeDouble(val);
    return pb_incr_float_raw(msg, fid, encoded_val, UnknownField::Type::TYPE_FIXED64);
}

static bool pb_update_string(std::shared_ptr<Message> msg, uint32_t fid, const std::string& val, bool force = false)
{
    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_sets = parse_unknow_field_sets(sets);
    auto iter = child_sets.find(fid);
    if (iter == child_sets.end()) {
        if (force) {
            sets->AddLengthDelimited(fid, val);
            return true;
        }
        return false;
    }

    std::vector<UnknownField*>& fields = iter->second;
    if (fields.size() != 1) {
        return false;
    }

    UnknownField* field = fields[0];
    if (field->type() != UnknownField::Type::TYPE_LENGTH_DELIMITED) {
        return false;
    }

    field->set_length_delimited(val);
    return true;
}

static bool pb_incr_int(std::shared_ptr<Message> msg, uint32_t fid, int64_t val)
{
    return pb_incr_int_raw(msg, fid, val);
}

static std::string unknown_field_set_encode(UnknownFieldSet* sets)
{
	std::string str;
    // Map order is not deterministic. To make the test deterministic we want
    // to serialize the proto deterministically.
    io::StringOutputStream output(&str);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    WireFormat::SerializeUnknownFields(*sets, &out);
    //WireFormatLite
    out.Trim();

    return std::move(str);
}

static bool parse_protobuf_message(const std::string& data, std::unordered_map<uint32, UnknownField*>& fid_set) {
    io::CodedInputStream input((const uint8*)data.data(), data.size());
    using WireType = internal::WireFormatLite::WireType;
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        uint32 number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          UnknownField* field = new UnknownField();
          field->number_ = number;
          field->SetType(UnknownField::TYPE_VARINT);
          field->data_.varint_ = value;
          fid_set.emplace(number, field);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          // UnknownField field;
          // field.number_ = number;
          // field.SetType(UnknownField::TYPE_FIXED32);
          // field.data_.fixed32_ = value;
          // fid_set.emplace(number, field);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string str;
          bool rc = input.ReadString(&str, size);
          // UnknownField field;
          // field.number_ = number;
          // field.SetType(UnknownField::TYPE_LENGTH_DELIMITED);
          // field.data_.length_delimited_.string_value = new std::string;
          // field.data_.length_delimited_.string_value->assign(str);
          // fid_set.emplace(number, field);
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          // UnknownField field;
          // field.number_ = number;
          // field.SetType(UnknownField::TYPE_FIXED64);
          // field.data_.fixed64_ = value;
          // fid_set.emplace(number, field);
          break;
        }
        default:
          break;
      }
    }
    return true;
}

class FieldCmd {
public:
    enum Type {
        UNKNWON_CMD = 0,
        UPDATE_VARINT = 1,
        UPDATE_FIXED32 = 2,
        UPDATE_FIXED64 = 3,
        UPDATE_LENGTH_DELIMITED = 4,
        INCR_VARINT = 5,
        INCR_FLOAT = 6,
        INCR_DOUBLE = 7,
        LADD_MESSAGE = 8,
        ZADD_MESSAGE = 9
    };

    FieldCmd() : type_(Type::UNKNWON_CMD) { data_.varint_ = 0; }

public:
    void Delete() {
        if (type_ == Type::UPDATE_LENGTH_DELIMITED || type_ == LADD_MESSAGE || type_ == ZADD_MESSAGE) {
            if (!data_.length_delimited_.string_value) return;
            delete data_.length_delimited_.string_value;
            data_.length_delimited_.string_value = nullptr;
        }
    }

public:
    union LengthDelimited {
        std::string* string_value;
    };

    uint32 type_;
    union {
        uint64 varint_;
        uint32 fixed32_;
        uint64 fixed64_;
        mutable union LengthDelimited length_delimited_;
    } data_;
};

class FieldCmdSet {
public:
    FieldCmd* fieldCmd(uint32 fid) {
        auto iter = fieldCmdSet.find(fid);
        if (iter == fieldCmdSet.end()) return nullptr;
        return &iter->second;
    }

    ~FieldCmdSet() {
        clear();
    }

    void updateVarint(int32_t number, uint64 value) {
        FieldCmd cmd;
        cmd.type_ = FieldCmd::Type::UPDATE_VARINT;
        cmd.data_.varint_ = value;
        fieldCmdSet.emplace(number, cmd);
    }
    void updateFixed32(int32_t number, uint32 value) {
        FieldCmd cmd;
        cmd.type_ = FieldCmd::Type::UPDATE_FIXED32;
        cmd.data_.fixed32_ = value;
        fieldCmdSet.emplace(number, cmd);
    }
    void updateFixed64(int32_t number, uint64 value) {
        FieldCmd cmd;
        cmd.type_ = FieldCmd::Type::UPDATE_FIXED64;
        cmd.data_.fixed32_ = value;
        fieldCmdSet.emplace(number, cmd);
    }

private:
    void clear() {
        for (auto& item : fieldCmdSet) {
            auto& field_cmd = item.second;
            field_cmd.Delete();
        }
    }

public:
    std::unordered_map<uint32_t, FieldCmd> fieldCmdSet;
};

static bool pb_update(const std::string& idata, FieldCmdSet& field_cmd_set, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    using WireType = internal::WireFormatLite::WireType;
    using WireFormatLite = internal::WireFormatLite;
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          FieldCmd* cmd = field_cmd_set.fieldCmd(number);
          if (cmd && cmd->type_ == FieldCmd::Type::UPDATE_VARINT) {
            value = cmd->data_.varint_;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
          out.WriteVarint64(value);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          FieldCmd* cmd = field_cmd_set.fieldCmd(number);
          if (cmd && cmd->type_ == FieldCmd::Type::UPDATE_FIXED64) {
            value = cmd->data_.fixed64_;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED64));
          out.WriteLittleEndian64(value);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string str;
          bool rc = input.ReadString(&str, size);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
          out.WriteVarint32(str.size());
          out.WriteRawMaybeAliased(str.data(), str.size());
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          FieldCmd* cmd = field_cmd_set.fieldCmd(number);
          if (cmd && cmd->type_ == FieldCmd::Type::UPDATE_FIXED32) {
            value = cmd->data_.fixed32_;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
          out.WriteLittleEndian32(value);
          break;
        }
        default:
          break;
      }
    }
    out.Trim();

    return true;
}

enum CmdType {
    UNKNWON_CMD = 0,
    UPDATE_VARINT = 1,
    UPDATE_FIXED32 = 2,
    UPDATE_FIXED64 = 3,
    UPDATE_LENGTH_DELIMITED = 4,
    INCR_VARINT = 5,
    INCR_FLOAT = 6,
    INCR_DOUBLE = 7,
    LADD_MESSAGE = 8,
    ZADD_MESSAGE = 9
};

static bool pb_update_int2(const std::string& idata, int32_t fid, std::pair<CmdType, uint64> val, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    using WireType = internal::WireFormatLite::WireType;
    using WireFormatLite = internal::WireFormatLite;
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          if (fid == number) {
            if (val.first != CmdType::UPDATE_VARINT) {
                return false;
            }
            value = val.second;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
          out.WriteVarint64(value);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          if (fid == number) {
            if (val.first != CmdType::UPDATE_FIXED64) {
                return false;
            }
            value = val.second;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED64));
          out.WriteLittleEndian64(value);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          if (fid == number) {
            return false;
          }

          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string str;
          bool rc = input.ReadString(&str, size);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
          out.WriteVarint32(str.size());
          out.WriteRawMaybeAliased(str.data(), str.size());
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          if (fid == number) {
            if (val.first != CmdType::UPDATE_FIXED32) {
                return false;
            }
            value = (uint32)val.second;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
          out.WriteLittleEndian32(value);
          break;
        }
        default:
          break;
      }
    }
    out.Trim();

    return true;
}

union fdata_ {
    uint64 varint_;
    uint32 fixed32_;
    uint64 fixed64_;
    mutable std::string* length_delimited_;
};

static bool pb_update_val3(const std::string& idata, int32_t ftag, fdata_ val, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    using WireType = internal::WireFormatLite::WireType;
    using WireFormatLite = internal::WireFormatLite;
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          if (ftag == tag) {
            value = val.varint_;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
          out.WriteVarint64(value);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          if (ftag == tag) {
            value = val.fixed64_;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED64));
          out.WriteLittleEndian64(value);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string str;
          bool rc = input.ReadString(&str, size);
          if (ftag == tag) {
            out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
            out.WriteVarint32(val.length_delimited_->size());
            out.WriteRawMaybeAliased(val.length_delimited_->data(), val.length_delimited_->size());
          } else {
            out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
            out.WriteVarint32(str.size());
            out.WriteRawMaybeAliased(str.data(), str.size());
          }
          
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          if (ftag == tag) {
            value = val.fixed32_;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
          out.WriteLittleEndian32(value);
          break;
        }
        default:
          break;
      }
    }
    out.Trim();

    return true;
}

static bool pb_incr_val3(const std::string& idata, int32_t ftag, fdata_ val, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    using WireType = internal::WireFormatLite::WireType;
    using WireFormatLite = internal::WireFormatLite;
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          if (ftag == tag) {
            value += (int64_t)val.varint_;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
          //fprintf(stderr, "%d\n", number);
          out.WriteVarint64(value);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          if (ftag == tag) {
            value += (int64_t)val.fixed64_;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED64));
          out.WriteLittleEndian64(value);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string str;
          bool rc = input.ReadString(&str, size);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
          out.WriteVarint32(str.size());
          out.WriteRawMaybeAliased(str.data(), str.size());
          
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          if (ftag == tag) {
            value += (int32_t)val.fixed32_;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
          out.WriteLittleEndian32(value);
          break;
        }
        default:
          break;
      }
    }
    out.Trim();

    return true;
}

using WireType = internal::WireFormatLite::WireType;
using WireFormatLite = internal::WireFormatLite;

static bool parse_message(uint32_t ptag, std::string& idata, fdata_& data)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;
        //fprintf(stderr, "number: %d\n", number);

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          if (ptag == tag) {
            data.varint_ = value;
            return true;
          }
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          if (ptag == tag) {
            data.varint_ = value;
            return true;
          }
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string* str = new std::string();
          bool rc = input.ReadString(str, size);
          if (ptag == tag) {
            data.length_delimited_ = str;
            return true;
          }
          
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          if (ptag == tag) {
            data.varint_ = value;
            return true;
          }
          break;
        }
        default:
          break;
      }
    }
    return true;
}

static bool pb_list_add3(const std::string& idata, int32_t ftag, fdata_ val, int32_t ptag, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    std::map<uint64_t, std::string*> sstr;
    fdata_ data_;
    parse_message(ptag, *val.length_delimited_, data_);
    sstr.emplace(data_.varint_, val.length_delimited_);
    //fprintf(stderr, "%lu\n", data_.varint_);

    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
          //fprintf(stderr, "%d\n", number);
          out.WriteVarint64(value);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED64));
          out.WriteLittleEndian64(value);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string* str = new std::string();
          bool rc = input.ReadString(str, size);
          if (ftag == tag) {
            fdata_ dvalue;
            parse_message(ptag, *str, dvalue);
            if (ptag & 7 != WireType::WIRETYPE_LENGTH_DELIMITED) {
                sstr.emplace(dvalue.varint_, str);
                //fprintf(stderr, "%lu\n", dvalue.varint_);
            }
          } else {
            out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
            out.WriteVarint32(str->size());
            out.WriteRawMaybeAliased(str->data(), str->size());
            delete str;
          }

          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
          out.WriteLittleEndian32(value);
          break;
        }
        default:
          break;
      }
    }
    for (auto& item : sstr) {
        out.WriteVarint32(WireFormatLite::MakeTag(ftag >> 3, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
        out.WriteVarint32(item.second->size());
        out.WriteRawMaybeAliased(item.second->data(), item.second->size());
    }

    out.Trim();

    return true;
}

static bool pb_incr_int2(const std::string& idata, int32_t fid, int64 val, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    using WireType = internal::WireFormatLite::WireType;
    using WireFormatLite = internal::WireFormatLite;
    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
          uint64 value;
          bool rc = input.ReadVarint64(&value);
          if (fid == number) {
            value += val;
          }
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
          out.WriteVarint64(value);
          break;
        }
        case WireType::WIRETYPE_FIXED64: {
          uint64 value;
          bool rc = input.ReadLittleEndian64(&value);
          if (fid == number) {
            return false;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED64));
          out.WriteLittleEndian64(value);
          break;
        }
        case WireType::WIRETYPE_LENGTH_DELIMITED: {
          if (fid == number) {
            return false;
          }

          uint32 size;
          if (!input.ReadVarint32(&size)) {
            break;
          }
          std::string str;
          bool rc = input.ReadString(&str, size);
          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
          out.WriteVarint32(str.size());
          out.WriteRawMaybeAliased(str.data(), str.size());
          break;
        }
        case WireType::WIRETYPE_FIXED32: {
          uint32 value;
          bool rc = input.ReadLittleEndian32(&value);
          if (fid == number) {
            return false;
          }

          out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
          out.WriteLittleEndian32(value);
          break;
        }
        default:
          break;
      }
    }
    out.Trim();

    return true;
}

using WireType = internal::WireFormatLite::WireType;
using WireFormatLite = internal::WireFormatLite;

TEST(Protobuf3Api2Test, test_pb_update)
{
    //fprintf( stderr, "111 %lu\n", TimeHelper::currentUs() );
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(2);
    message.set_id2(3);
    message.set_id3(4);
    message.set_id4(5);
    message.set_id5(6);
    message.set_id6(7);
    message.set_id7(8);
    message.set_id8(9);
    message.set_id9(10);
    message.set_id10(11);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    fprintf( stderr, "%s\n", message.DebugString().c_str() );

    //fprintf(stderr, "222 %lu\n", TimeHelper::currentUs());
    // raw decode
    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");

    EXPECT_TRUE( pool.BuildFile(file) != nullptr );

    const Descriptor* descriptor = pool.FindMessageTypeByName("EmptyMessage");
    EXPECT_TRUE( descriptor != nullptr );

    DynamicMessageFactory dynamic_factory(&pool);

    //fprintf(stderr, "333 %lu\n", TimeHelper::currentUs());
    std::shared_ptr<Message> msg(dynamic_factory.GetPrototype(descriptor)->New());

    EXPECT_TRUE( msg->ParsePartialFromString(value) );
    EXPECT_TRUE( msg->IsInitialized() );

    //fprintf(stderr, "444 %lu\n", TimeHelper::currentUs());
    fprintf(stderr, "%s\n", msg->DebugString().c_str());

    EXPECT_TRUE( pb_update_int(msg, 2, 22) );
    EXPECT_TRUE( pb_update_int(msg, 3, 33) );
    EXPECT_TRUE( pb_update_int(msg, 4, 4294967295) );
    EXPECT_TRUE( pb_update_int(msg, 5, 18446744073709551615) );
    EXPECT_FALSE( pb_update_int(msg, 8, 22) );
    EXPECT_FALSE( pb_update_int(msg, 9, 33) );
    EXPECT_FALSE( pb_update_int(msg, 10, 22) );
    EXPECT_FALSE( pb_update_int(msg, 11, 33) );
    EXPECT_FALSE( pb_update_int(msg, 14, 22) );
    EXPECT_FALSE( pb_update_int(msg, 15, 33) );
    EXPECT_FALSE( pb_update_int(msg, 1, 33) );

    EXPECT_TRUE( pb_update_sint(msg, 6, -10000) );
    EXPECT_TRUE( pb_update_sint(msg, 7, -10000000) );
    EXPECT_FALSE( pb_update_sint(msg, 8, 22) );
    EXPECT_FALSE( pb_update_sint(msg, 9, 33) );
    EXPECT_FALSE( pb_update_sint(msg, 10, 22) );
    EXPECT_FALSE( pb_update_sint(msg, 11, 33) );
    EXPECT_FALSE( pb_update_sint(msg, 14, 22) );
    EXPECT_FALSE( pb_update_sint(msg, 15, 33) );
    EXPECT_FALSE( pb_update_sint(msg, 1, 33) );

    EXPECT_TRUE( pb_update_bool(msg, 12, true) );
    EXPECT_FALSE( pb_update_bool(msg, 8, true) );
    EXPECT_FALSE( pb_update_bool(msg, 9, true) );
    EXPECT_FALSE( pb_update_bool(msg, 10, true) );
    EXPECT_FALSE( pb_update_bool(msg, 11, true) );
    EXPECT_FALSE( pb_update_bool(msg, 14, true) );
    EXPECT_FALSE( pb_update_bool(msg, 15, true) );
    EXPECT_FALSE( pb_update_bool(msg, 1, true) );

    EXPECT_TRUE( pb_update_string(msg, 1, "test") );
    EXPECT_FALSE( pb_update_string(msg, 8, "test") );
    EXPECT_FALSE( pb_update_string(msg, 9, "test") );
    EXPECT_FALSE( pb_update_string(msg, 10, "test") );
    EXPECT_FALSE( pb_update_string(msg, 11, "test") );
    EXPECT_FALSE( pb_update_string(msg, 14, "test") );
    EXPECT_FALSE( pb_update_string(msg, 15, "test") );

    EXPECT_TRUE( pb_update_fixed32(msg, 8, 4294967295) );
    EXPECT_TRUE( pb_update_fixed32(msg, 10, 4294967295) );
    EXPECT_FALSE( pb_update_fixed32(msg, 9, 4294967295) );
    EXPECT_FALSE( pb_update_fixed32(msg, 11, 4294967295) );
    EXPECT_FALSE( pb_update_fixed32(msg, 14, 111) );
    EXPECT_FALSE( pb_update_fixed32(msg, 1, 111) );

    EXPECT_TRUE( pb_update_fixed64(msg, 9, 18446744073709551615) );
    EXPECT_TRUE( pb_update_fixed64(msg, 11, 18446744073709551615) );
    EXPECT_FALSE( pb_update_fixed64(msg, 8, 111) );
    EXPECT_FALSE( pb_update_fixed64(msg, 10, 111) );
    EXPECT_FALSE( pb_update_fixed64(msg, 15, 111) );
    EXPECT_FALSE( pb_update_fixed64(msg, 1, 111) );

    EXPECT_TRUE( pb_update_double(msg, 14, 3.111111) );
    EXPECT_TRUE( pb_update_float(msg, 15, 5.111111) );
    EXPECT_FALSE( pb_update_float(msg, 14, 3.111111) );
    EXPECT_FALSE( pb_update_double(msg, 15, 5.111111) );

    fprintf(stderr, "%s\n", msg->DebugString().c_str());

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( msg->SerializePartialToString(&value) );
    //fprintf( stderr, "555 %lu\n", TimeHelper::currentUs() );

    EXPECT_TRUE( message2.ParseFromString(value) );

    fprintf( stderr, "%s\n", message2.DebugString().c_str() );
    //fprintf( stderr, "666 %lu\n", TimeHelper::currentUs() );
}

TEST(Protobuf3Api2Test, test_pb_incr)
{
    //fprintf( stderr, "111 %lu\n", TimeHelper::currentUs() );
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(2);
    message.set_id2(3);
    message.set_id3(4);
    message.set_id4(5);
    message.set_id5(6);
    message.set_id6(7);
    message.set_id7(8);
    message.set_id8(9);
    message.set_id9(10);
    message.set_id10(11);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    fprintf( stderr, "%s\n", message.DebugString().c_str() );

    //fprintf(stderr, "222 %lu\n", TimeHelper::currentUs());
    // raw decode
    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");

    EXPECT_TRUE( pool.BuildFile(file) != nullptr );

    const Descriptor* descriptor = pool.FindMessageTypeByName("EmptyMessage");
    EXPECT_TRUE( descriptor != nullptr );

    DynamicMessageFactory dynamic_factory(&pool);

    //fprintf(stderr, "333 %lu\n", TimeHelper::currentUs());
    std::shared_ptr<Message> msg(dynamic_factory.GetPrototype(descriptor)->New());

    EXPECT_TRUE( msg->ParsePartialFromString(value) );
    EXPECT_TRUE( msg->IsInitialized() );

    //fprintf(stderr, "444 %lu\n", TimeHelper::currentUs());
    fprintf(stderr, "%s\n", msg->DebugString().c_str());

    EXPECT_TRUE( pb_incr_int(msg, 2, 4294967293) );
    EXPECT_TRUE( pb_incr_int(msg, 3, 18446744073709551612) );
    EXPECT_TRUE( pb_incr_int(msg, 4, 4294967291) );
    EXPECT_TRUE( pb_incr_int(msg, 5, 18446744073709551610) );

    EXPECT_TRUE( pb_incr_float(msg, 15, 1.222) );
    EXPECT_TRUE( pb_incr_double(msg, 14, 2.1111) );

    fprintf(stderr, "%s\n", msg->DebugString().c_str());

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( msg->SerializePartialToString(&value) );
    //fprintf( stderr, "555 %lu\n", TimeHelper::currentUs() );

    EXPECT_TRUE( message2.ParseFromString(value) );

    fprintf( stderr, "%s\n", message2.DebugString().c_str() );
    //fprintf( stderr, "666 %lu\n", TimeHelper::currentUs() );
}

bool pb_update_int3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.varint_ = value;
    return pb_update_val3(input, (number << 3) | WireType::WIRETYPE_VARINT, val, output);
}

bool pb_update_sint3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    uint64_t encoded_val = WireFormatLite::ZigZagEncode64(value);
    return pb_update_int3(input, number, encoded_val, output);
}

bool pb_update_string3(std::string& input, int32_t number, std::string& value, std::string& output)
{
    fdata_ val;
    val.length_delimited_ = &value;
    return pb_update_val3(input, (number << 3) | WireType::WIRETYPE_LENGTH_DELIMITED, val, output);
}

bool pb_update_fixed32_3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed32_ = value;
    return pb_update_val3(input, (number << 3) | WireType::WIRETYPE_FIXED32, val, output);
}

bool pb_update_fixed64_3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed64_ = value;
    return pb_update_val3(input, (number << 3) | WireType::WIRETYPE_FIXED64, val, output);
}

static bool pb_update_float3(std::string& input, int32_t number, float val, std::string& output)
{
    uint32_t encoded_val = WireFormatLite::EncodeFloat(val);
    return pb_update_fixed32_3(input, number, encoded_val, output);
}

static bool pb_update_double3(std::string& input, int32_t number, double val, std::string& output)
{
    uint64_t encoded_val = WireFormatLite::EncodeDouble(val);
    return pb_update_fixed64_3(input, number, encoded_val, output);
}

static bool pb_incr_int3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.varint_ = value;
    return pb_incr_val3(input, (number << 3) | WireType::WIRETYPE_VARINT, val, output);
}

static bool pb_incr_fixed32_3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed32_ = value;
    return pb_incr_val3(input, (number << 3) | WireType::WIRETYPE_FIXED32, val, output);
}

static bool pb_incr_fixed64_3(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed64_ = value;
    return pb_incr_val3(input, (number << 3) | WireType::WIRETYPE_FIXED64, val, output);
}

static bool pb_list_add3_by_fixed32(std::string& input, int32_t fnumber, std::string* value, int32_t pnumber, std::string& output)
{
    fdata_ val;
    val.length_delimited_ = value;
    int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    int32_t ptag = pnumber << 3 | WireType::WIRETYPE_FIXED32;
    return pb_list_add3(input, ftag, val, ptag, output);
}

TEST(Protobuf3Api2Test, test_pb_update3)
{
    //fprintf( stderr, "111 %lu\n", TimeHelper::currentUs() );
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(2);
    message.set_id2(3);
    message.set_id3(4);
    message.set_id4(5);
    message.set_id5(6);
    message.set_id6(7);
    message.set_id7(8);
    message.set_id8(9);
    message.set_id9(10);
    message.set_id10(11);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    fprintf( stderr, "%s\n", message.DebugString().c_str() );

    std::string output;
    EXPECT_TRUE( pb_update_int3(value, 3, 33, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_int3(value, 4, 4294967295, output) );
    value.assign(output); output.clear();

    // EXPECT_TRUE( pb_update_int2(value, 3, 33, output) );
    // EXPECT_TRUE( pb_update_int2(value, 4, 4294967295, output) );
    // EXPECT_TRUE( pb_update_int2(value, 5, 18446744073709551615, output) );
    // EXPECT_FALSE( pb_update_int2(value, 8, 22, output) );
    // EXPECT_FALSE( pb_update_int2(value, 9, 33, output) );
    // EXPECT_FALSE( pb_update_int2(value, 10, 22, output) );
    // EXPECT_FALSE( pb_update_int2(value, 11, 33, output) );
    // EXPECT_FALSE( pb_update_int2(value, 14, 22, output) );
    // EXPECT_FALSE( pb_update_int2(value, 15, 33, output) );
    // EXPECT_FALSE( pb_update_int2(value, 1, 33, output) );

    EXPECT_TRUE( pb_update_sint3(value, 6, -10000, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_sint3(value, 7, -10000000, output) );
    value.assign(output); output.clear();
    // EXPECT_FALSE( pb_update_sint(msg, 8, 22) );
    // EXPECT_FALSE( pb_update_sint(msg, 9, 33) );
    // EXPECT_FALSE( pb_update_sint(msg, 10, 22) );
    // EXPECT_FALSE( pb_update_sint(msg, 11, 33) );
    // EXPECT_FALSE( pb_update_sint(msg, 14, 22) );
    // EXPECT_FALSE( pb_update_sint(msg, 15, 33) );
    // EXPECT_FALSE( pb_update_sint(msg, 1, 33) );

    // EXPECT_TRUE( pb_update_bool(msg, 12, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 8, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 9, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 10, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 11, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 14, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 15, true) );
    // EXPECT_FALSE( pb_update_bool(msg, 1, true) );

    std::string val("test");
    EXPECT_TRUE( pb_update_string3(value, 1, val, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_string3(value, 8, val, output) );
    value.assign(output); output.clear();

    // EXPECT_FALSE( pb_update_string(msg, 8, "test") );
    // EXPECT_FALSE( pb_update_string(msg, 9, "test") );
    // EXPECT_FALSE( pb_update_string(msg, 10, "test") );
    // EXPECT_FALSE( pb_update_string(msg, 11, "test") );
    // EXPECT_FALSE( pb_update_string(msg, 14, "test") );
    // EXPECT_FALSE( pb_update_string(msg, 15, "test") );

    EXPECT_TRUE( pb_update_fixed32_3(value, 8, 4294967295, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_fixed32_3(value, 10, 4294967295, output) );
    value.assign(output); output.clear();
    // EXPECT_FALSE( pb_update_fixed32(msg, 9, 4294967295) );
    // EXPECT_FALSE( pb_update_fixed32(msg, 11, 4294967295) );
    // EXPECT_FALSE( pb_update_fixed32(msg, 14, 111) );
    // EXPECT_FALSE( pb_update_fixed32(msg, 1, 111) );

    EXPECT_TRUE( pb_update_fixed64_3(value, 9, 18446744073709551615, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_fixed64_3(value, 11, 18446744073709551615, output) );
    value.assign(output); output.clear();
    // EXPECT_TRUE( pb_update_fixed64(msg, 9, 18446744073709551615) );
    // EXPECT_TRUE( pb_update_fixed64(msg, 11, 18446744073709551615) );
    // EXPECT_FALSE( pb_update_fixed64(msg, 8, 111) );
    // EXPECT_FALSE( pb_update_fixed64(msg, 10, 111) );
    // EXPECT_FALSE( pb_update_fixed64(msg, 15, 111) );
    // EXPECT_FALSE( pb_update_fixed64(msg, 1, 111) );

    EXPECT_TRUE( pb_update_double3(value, 14, 3.111111, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_float3(value, 15, 5.111111, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_float3(value, 14, 3.111111, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_double3(value, 15, 5.111111, output) );
    value.assign(output); output.clear();

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );

    fprintf( stderr, "%s\n", message2.DebugString().c_str() );
}

static void pb_raw_print(const std::string& str)
{
    uint8_t* ch = (uint8_t*)str.data();
    uint32_t size = str.size();
    //fprintf(stdout, "raw_print[%d]: ", str.size());

    for (uint32_t i = 0; i < size; i++) {
        fprintf(stdout, "%02x ", ch[i]);
        // char s[10];
        // snprintf(s, sizeof(s), "%d", ch[i]);
        // fprintf(stdout, "%s ", s);
    }
    fprintf(stdout, "\n");
}

TEST(Protobuf3Api2Test, test_pb_incr3)
{
    //fprintf( stderr, "111 %lu\n", TimeHelper::currentUs() );
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(2);
    message.set_id2(3);
    message.set_id3(4);
    message.set_id4(5);
    message.set_id5(6);
    message.set_id6(7);
    message.set_id7(8);
    message.set_id8(9);
    message.set_id9(10);
    message.set_id10(11);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );
    fprintf( stderr, "%s\n", message.DebugString().c_str() );

    std::string output;
    EXPECT_TRUE( pb_incr_int3(value, 2, 4294967293, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_incr_int3(value, 3, -1, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_incr_int3(value, 4, -4, output) );
    value.assign(output); output.clear();
    // EXPECT_TRUE( pb_incr_int(msg, 3, 18446744073709551612) );
    // EXPECT_TRUE( pb_incr_int(msg, 4, 4294967291) );
    // EXPECT_TRUE( pb_incr_int(msg, 5, 18446744073709551610) );

    pb_incr_fixed32_3(value, 8, -1, output);
    value.assign(output); output.clear();
    pb_incr_fixed32_3(value, 10, -1, output);
    value.assign(output); output.clear();
    pb_incr_fixed64_3(value, 9, -1, output);
    value.assign(output); output.clear();
    pb_incr_fixed64_3(value, 11, -1, output);

    // EXPECT_TRUE( pb_incr_float(msg, 15, 1.222) );
    // EXPECT_TRUE( pb_incr_double(msg, 14, 2.1111) );

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );

    fprintf( stderr, "%s\n", message2.DebugString().c_str() );
    //fprintf( stderr, "666 %lu\n", TimeHelper::currentUs() );
}

TEST(Protobuf3Api2Test, test_pb_list_add3)
{
    //fprintf( stderr, "111 %lu\n", TimeHelper::currentUs() );
    unit_test::ExampleApiMessage message;

    unit_test::ExampleApiMessage::EmbeddedMsg* emsg = message.add_sub_msg2();
    emsg->set_number(1);
    emsg->set_info("info1");
    emsg = message.add_sub_msg2();
    emsg->set_number(2);
    emsg->set_info("info2");

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );
    fprintf( stderr, "%s\n", message.DebugString().c_str() );

    pb_raw_print(value);

    unit_test::ExampleApiMessage::EmbeddedMsg emsg2;
    emsg2.set_number(3);
    emsg2.set_info("info3");
    std::string* value2 = new std::string();
    emsg2.SerializeToString(value2);

    std::string output;
    pb_list_add3_by_fixed32(value, 32, value2, 1, output);

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );
    pb_raw_print(output);

    fprintf( stderr, "%s\n", message2.DebugString().c_str() );
}

/*
1. value.size: 100
   update: 34210   -> 14636   -58%
   setadd: 60042   -> 26113   -57%
2. value.size: 1990
   update: 592851  -> 95741   -84%
   setadd: 1104689 -> 403623  -63%
3. value.size: 3910
   update: 1145193 -> 180760  -85%
   setadd: 2099790 -> 798004  -62%

单个请求平均处理时间(us)：
1. value.size: 100
   update: 3.42   -> 1.46   -58%   (-51%)
   setadd: 6      -> 2.6    -57%   (-51%)
2. value.size: 1990
   update: 59.2    -> 9.57   -84%  (-58%)
   setadd: 110.46  -> 40.36  -63%  (-55%)
3. value.size: 3910
   update: 114.51 -> 18.07  -85%   (-63%)
   setadd: 209.97 -> 79.8   -62%   (-57%)

内网ping时延：100us左右 (网络时延开销、网络io开销)
*/
TEST(Protobuf3Api2Test, test_pb_benchmark)
{
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(2);
    message.set_id2(3);
    message.set_id3(4);
    message.set_id4(5);
    message.set_id5(6);
    message.set_id6(7);
    message.set_id7(8);
    message.set_id8(9);
    message.set_id9(10);
    message.set_id10(11);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    for (uint32 i = 0; i < 128; i++) {
        unit_test::ExampleApiMessage::EmbeddedMsg* emsg = message.add_sub_msg2();
        emsg->set_number(i);
        emsg->set_info("info1");
    }

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");

    EXPECT_TRUE( pool.BuildFile(file) != nullptr );

    const Descriptor* descriptor = pool.FindMessageTypeByName("EmptyMessage");
    EXPECT_TRUE( descriptor != nullptr );

    fprintf(stderr, "value.size: %d\n", value.size());
    DynamicMessageFactory dynamic_factory(&pool);

    uint32_t total_times = 100;

    {
        uint64_t start_ts = TimeHelper::currentUs();

        for (uint32_t i = 0; i < total_times; i++) {
            unit_test::ExampleApiMessage message2;
            EXPECT_TRUE( message2.ParseFromString(value) );
            //EXPECT_TRUE( message2.IsInitialized() );
            //fprintf( stderr, "ts1: %lu\n", TimeHelper::currentUs() - start_ts );

            message2.set_id9(20);
            message2.set_id10(21);
            std::string value2;
            EXPECT_TRUE( message.SerializeToString(&value2) );
        }

        fprintf( stderr, "ts1: %lu message: \n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();
        std::shared_ptr<Message> msg(dynamic_factory.GetPrototype(descriptor)->New());
        fprintf( stderr, "ts2: %lu\n", TimeHelper::currentUs() - start_ts );

        EXPECT_TRUE( msg->ParsePartialFromString(value) );
        //EXPECT_TRUE( msg->IsInitialized() );
        fprintf( stderr, "ts2: %lu\n", TimeHelper::currentUs() - start_ts );

        EXPECT_TRUE( pb_update_fixed32(msg, 8, 4294967295) );
        //EXPECT_TRUE( pb_update_fixed32(msg, 10, 4294967295) );
        fprintf( stderr, "ts2: %lu\n", TimeHelper::currentUs() - start_ts );

        std::string value2;
        EXPECT_TRUE( msg->SerializePartialToString(&value2) );

        fprintf( stderr, "ts2: %lu\n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();
        unit_test::ExampleApiMessage message2;

        for (uint32_t i = 0; i < total_times; i++) {
            std::string output;
            //pb_update_int2(value, 2, {CmdType::UPDATE_VARINT, 100}, output);
            pb_incr_int3(value, 2, 100, output);
            //EXPECT_TRUE( message2.ParseFromString(output) );
        }

        fprintf( stderr, "ts3: %lu message2: %s\n", TimeHelper::currentUs() - start_ts, message2.DebugString().c_str() );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();

        for (uint32_t i = 0; i < total_times; i++) {
            unit_test::ExampleApiMessage message2;
            EXPECT_TRUE( message2.ParseFromString(value) );
            
            unit_test::ExampleApiMessage message_2 = message2;
            message_2.clear_sub_msg2();

            std::map<uint64_t, unit_test::ExampleApiMessage_EmbeddedMsg> smap;

            int32_t size = message2.sub_msg2_size();
            for (int32_t i = 0; i < size; i++) {
                smap.emplace(message2.sub_msg2(i).number(), message2.sub_msg2(i));
            }

            unit_test::ExampleApiMessage_EmbeddedMsg tmp;
            tmp.set_number(3);
            tmp.set_info("info3");
            smap.emplace(tmp.number(), tmp);

            for (auto& item : smap) {
                unit_test::ExampleApiMessage_EmbeddedMsg* tmp2 = message_2.add_sub_msg2();
                *tmp2 = item.second;
            }

            std::string value2;
            EXPECT_TRUE( message_2.SerializeToString(&value2) );
        }
        //fprintf( stderr, "ts3: %lu message2: %s\n", TimeHelper::currentUs() - start_ts, message_2.DebugString().c_str() );
        fprintf( stderr, "ts4: %lu\n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();
        //unit_test::ExampleApiMessage message2 = message;

        for (uint32_t i = 0; i < total_times; i++) {
            unit_test::ExampleApiMessage::EmbeddedMsg emsg2;
            emsg2.set_number(512);
            emsg2.set_info("info3");
            std::string* value2 = new std::string();
            emsg2.SerializeToString(value2);

            std::string output;
            pb_list_add3_by_fixed32(value, 32, value2, 1, output);
        }

        fprintf( stderr, "ts5: %lu\n", TimeHelper::currentUs() - start_ts );
    }
}
