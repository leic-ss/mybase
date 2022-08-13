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

// repeated_pb_add(key, fid, int32_t)
// repeated_pb_add(key, fid, string)
// repeated_pb_del(key, fid, int32_t)
// repeated_pb_del(key, fid, string)
// repeated_pb_list(key, fid, std::vector<int32_t>& )
// repeated_pb_list(key, fid, std::vector<string>& )

// pb_del(key, fid)

// pb_update(key, fid, int32_t)
// pb_update(key, fid, string)

// pb_read_int(key, fid)
// pb_read_string(key, fid)

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

// proto3
// proto2 packed
// proto2 no packed

// pb_add_varint
// pb_add_fix32
// pb_add_fix64
// pb_add_string

// message {1}, pb_add_varint(msg, "2", 1) -> success as 2 is not exist
// message {1}, pb_add_varint(msg, "2.1", 1) -> failed as 2 is not exist
// message {1, 2}, pb_add_varint(msg, "1.1", 1) -> failed as 1 is primitive
// message {1, 2}, pb_add_varint(msg, "1.1", 1) -> success as 1 is repeated primitive4

// message {1, 2, 3 : {1}}, pb_add_varint(msg, "3.1", 1) -> success as 1.1 is repeated primitive
// message {1, 2, 3 : {1}}, pb_add_varint(msg, "3.2", 1) -> success
static bool pb_add_raw_primitive(std::shared_ptr<Message> msg, const std::string& fields, UnknownField& field_to_add)
{
	const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());
	std::vector<std::string> fids = StringHelper::tokenize(fields, ".");

	std::vector<std::pair<UnknownField*, UnknownFieldSet*>> embedded_childs;

    bool success = true;
    uint32_t fid_size = fids.size();
    for (uint32_t i = 0; i < fid_size; i++) {
        int32_t field_id = std::stoi(fids[i]);
        if (field_id < 0) {
            fprintf(stderr, "field id format failed, must be a number! id[%s]\n", fids[i].c_str());
            return false;
        }

        auto cur_fields = parse_unknow_field_sets(sets);
        auto iter = cur_fields.find(field_id);
        // INFO: add new field, do not care about type
        if (iter == cur_fields.end() && i == fid_size - 1) {
            field_to_add.number_ = field_id;
            sets->AddField(field_to_add);
            break;
        } else if (iter == cur_fields.end() && i < fid_size - 1) {
            // INFO: add new field with child field, not be allowed
            // EXMP: message {1}, pb_add_varint(msg, "2.1", 1) -> failed as 2 is not exist
            success = false;
            break;
        }

        auto& vec = iter->second;
        if (vec.size() > 1 || vec.empty()) {
            // INFO:  it is repeated string | repeeated embedded message
            success = false;
            break;
        }

        // INFO: it is single primitive | repeated primitive | embedded message | single string
        UnknownField* field = vec[0];
        if (field->type() != UnknownField::TYPE_LENGTH_DELIMITED) {
            // INFO: it is single primitive type
            // TODO: free memory
            success = false;
            break;
        }

        // INFO: it is repeated primitive | embedded message | single string

        UnknownFieldSet* embedded_unknown_field_sets = new UnknownFieldSet();

        const std::string& value = field->length_delimited();
        bool is_embedded_message = (!value.empty() && embedded_unknown_field_sets->ParseFromString(value));
        if (is_embedded_message && i < fid_size - 1) {
            // INFO: it is embedded message
            embedded_childs.push_back( std::make_pair(field, embedded_unknown_field_sets) );
            sets = embedded_unknown_field_sets;
        } else if (is_embedded_message && i == fid_size - 1) {
            delete embedded_unknown_field_sets;
            // it is embedded message, and i == fid_size -1, can not add it as repeated primitive.
            success = false;
            break;
        } else if (iter != cur_fields.end() && i == fid_size - 1) {
            delete embedded_unknown_field_sets;

            // INFO: it is repeated primitive | single string
            // WARN: we can not know it is a string, still handle it
            // TODO: if it s string, it will be failed
            std::string str;
            io::StringOutputStream output(&str);
            CodedOutputStream out(&output);
            out.SetSerializationDeterministic(true);
            out.WriteRaw(field->length_delimited().data(), field->length_delimited().size());

            switch (field_to_add.type()) {
            case UnknownField::TYPE_VARINT: {
                WireFormatLite::WriteUInt64NoTag(field_to_add.varint(), &out);
                break;
            }
            case UnknownField::TYPE_FIXED32: {
                WireFormatLite::WriteFixed32NoTag(field_to_add.fixed32(), &out);
                break;
            }
            case UnknownField::TYPE_FIXED64: {
                WireFormatLite::WriteFixed64NoTag(field_to_add.fixed64(), &out);
                break;
            }
            case UnknownField::TYPE_LENGTH_DELIMITED: {
                fprintf(stderr, "it is error, skip it! type[%d]\n", field_to_add.type());
                break;
            }
            case UnknownField::TYPE_GROUP: {
                fprintf(stderr, "it is derelict!\n");
                break;
            }
            default:
              break;
            }

            out.Trim();
            field->set_length_delimited(str);
            break;
        } else {
            delete embedded_unknown_field_sets;

            // INFO: it is repeated primitive or may be string, and i < fid_size -1, can not add child field
            // TODO: free memory
            success = false;
            break;
        }
    }

    if ( !success ) {
        for (auto iter = embedded_childs.rbegin(); iter != embedded_childs.rend(); iter++) {
            auto embedded_field_sets = iter->second;
            delete embedded_field_sets;
        }
        return false;
    }

    // embedded_childs:
    // <root_field, embedded_message>
    //                      ->  <child_field, embedded_message>
    //                                                -> <child_field, embedded_message>
    //                                                                         -> ... ...
    for (auto iter = embedded_childs.rbegin(); iter != embedded_childs.rend(); iter++) {
        auto parent_field = iter->first;
        auto embedded_field_sets = iter->second;
        parent_field->set_length_delimited(unknown_field_set_encode(embedded_field_sets));

        delete embedded_field_sets;
    }

	return true;
}

static bool pb_add_raw_string(std::shared_ptr<Message> msg, const std::string& fields, const std::string& str_to_add)
{
    if (str_to_add.empty()) {
        return false;
    }
    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());
    std::vector<std::string> fids = StringHelper::tokenize(fields, ".");
    std::vector<std::pair<UnknownField*, UnknownFieldSet*>> embedded_childs;

    bool success = true;
    uint32_t fid_size = fids.size();
    for (uint32_t i = 0; i < fid_size; i++) {
        int32_t field_id = std::stoi(fids[i]);
        if (field_id < 0) {
            fprintf(stderr, "field id format failed, must be a number! id[%s]\n", fids[i].c_str());
            return false;
        }

        auto cur_fields = parse_unknow_field_sets(sets);
        auto iter = cur_fields.find(field_id);
        // INFO: add new field, do not care about type
        if (iter == cur_fields.end() && i == fid_size - 1) {
            sets->AddLengthDelimited(field_id, str_to_add);
            break;
        } else if (iter == cur_fields.end() && i < fid_size - 1) {
            // INFO: add new field with child field, not be allowed
            // EXMP: message {1}, pb_add_varint(msg, "2.1", 1) -> failed as 2 is not exist
            success = false;
            break;
        }

        auto& vec = iter->second;

        // INFO: it is single primitive
        if (vec.size() == 1 && vec[0]->type() != UnknownField::TYPE_LENGTH_DELIMITED) {
            // INFO: it is single primitive type
            // TODO: free memory
            success = false;
            break;
        }

        UnknownField* field = vec[0];

        // INFO: it is repeated primitive | single string | embedded message
        // INFO: it is repeated string | repeated embedded message

        if ( i == fid_size - 1 && vec.size() > 1) {
            // INFO: add it as repeated string

            break;
        } else if ( i <= fid_size - 1 && vec.size() > 1) {
            // INFO: add chield field for repeated string | repeated embedded message
            success = false;
            break;
        }

        // INFO: it is repeated primitive | single string | embedded message
        // INFO: it is repeated string with one element | repeated embedded message with one element

        UnknownFieldSet* embedded_unknown_field_sets = new UnknownFieldSet();

        const std::string& value = field->length_delimited();
        bool is_embedded_message = (!value.empty() && embedded_unknown_field_sets->ParseFromString(value));
        if (is_embedded_message && i < fid_size - 1) {
            // INFO: it is embedded message
            embedded_childs.push_back( std::make_pair(field, embedded_unknown_field_sets) );
            sets = embedded_unknown_field_sets;
        } else if (is_embedded_message && i == fid_size - 1) {
            delete embedded_unknown_field_sets;
            // INFO: it is embedded message | repeated embedded message with one element
            sets->AddLengthDelimited(field_id, str_to_add);
            break;
        } else if ( !is_embedded_message && i == fid_size - 1) {
            delete embedded_unknown_field_sets;

            // INFO: it is repeated primitive | single string | repeated string with one element
            sets->AddLengthDelimited(field_id, str_to_add);
            break;
        } else {
            delete embedded_unknown_field_sets;

            /// INFO: it is repeated primitive | single string | repeated string with one element,
            //        and i < fid_size -1, add child field is not allowed
            // TODO: free memory
            success = false;
            break;
        }
    }

    if ( !success ) {
        for (auto iter = embedded_childs.rbegin(); iter != embedded_childs.rend(); iter++) {
            auto embedded_field_sets = iter->second;
            delete embedded_field_sets;
        }
        return false;
    }

    // embedded_childs:
    // <root_field, embedded_message>
    //                      ->  <child_field, embedded_message>
    //                                                -> <child_field, embedded_message>
    //                                                                         -> ... ...
    for (auto iter = embedded_childs.rbegin(); iter != embedded_childs.rend(); iter++) {
        auto parent_field = iter->first;
        auto embedded_field_sets = iter->second;
        parent_field->set_length_delimited(unknown_field_set_encode(embedded_field_sets));

        delete embedded_field_sets;
    }

    return true;
}

bool pb_add_uint64(std::shared_ptr<Message> msg, const std::string& fields, uint64_t val)
{
    //pb_add_raw_primitive(msg, fields, field_to_add)
    UnknownField field_to_add;
    field_to_add.SetType(UnknownField::TYPE_VARINT);
    field_to_add.data_.varint_ = val;

    return pb_add_raw_primitive(msg, fields, field_to_add);
}

static bool pb_add_int32(std::shared_ptr<Message> msg, const std::string& fields, int32_t val)
{
    return pb_add_uint64(msg, fields, val);
}

static bool pb_add_int64(std::shared_ptr<Message> msg, const std::string& fields, int64_t val)
{
    return pb_add_uint64(msg, fields, val);
}

static bool pb_add_uint32(std::shared_ptr<Message> msg, const std::string& fields, uint32_t val)
{
    return pb_add_uint64(msg, fields, val);
}

static bool pb_add_sint32(std::shared_ptr<Message> msg, const std::string& fields, int32_t val)
{
    uint32_t encoded_val = WireFormatLite::ZigZagEncode32(val);
    return pb_add_uint64(msg, fields, encoded_val);
}

static bool pb_add_sint64(std::shared_ptr<Message> msg, const std::string& fields, int64_t val)
{
    uint64_t encoded_val = WireFormatLite::ZigZagEncode64(val);
    return pb_add_uint64(msg, fields, encoded_val);
}

static bool pb_add_fixed32(std::shared_ptr<Message> msg, const std::string& fields, uint32_t val)
{
    UnknownField field_to_add;
    field_to_add.SetType(UnknownField::TYPE_FIXED32);
    field_to_add.data_.fixed32_ = val;

    return pb_add_raw_primitive(msg, fields, field_to_add);
}

static bool pb_add_fixed64(std::shared_ptr<Message> msg, const std::string& fields, uint64_t val)
{
    UnknownField field_to_add;
    field_to_add.SetType(UnknownField::TYPE_FIXED64);
    field_to_add.data_.fixed64_ = val;

    return pb_add_raw_primitive(msg, fields, field_to_add);
}

static bool pb_add_float(std::shared_ptr<Message> msg, const std::string& fields, float val)
{
    uint32_t encoded_val = WireFormatLite::EncodeFloat(val);
    return pb_add_fixed32(msg, fields, encoded_val);
}

static bool pb_add_double(std::shared_ptr<Message> msg, const std::string& fields, double val)
{
    uint32_t encoded_val = WireFormatLite::EncodeDouble(val);
    return pb_add_fixed64(msg, fields, encoded_val);
}

static void pb_raw_print(const std::string& str)
{
    uint8_t* ch = (uint8_t*)str.data();
    uint32_t size = str.size();
    fprintf(stdout, "raw_print[%d]: ", str.size());

    for (uint32_t i = 0; i < size; i++) {
        fprintf(stdout, "%02x ", ch[i]);
    }
    fprintf(stdout, "\n");
}

/**
 * add primitive:
 * 1. field id is not exist, add it directly, success
 * 2. field id is exist and is primitive, failed
 * 3. field id is exist and is string. success (may be repeated primitive)             X
 * 4. field id is exist and is repeated string with only one element. success (may be repeated primitive)            X
 * 4. field id is exist and is repeated primitive. success
 * 4. field id is exist and is repeated string with more than two elements, failed
 * 4. field id is exist and is embedded_message, failed
 * 5. field id is exist and is repeated embedded_message with only one element, failed
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */
TEST(Protobuf3Test, test_pb_add_primitive)
{
    unit_test::Exp_Message message;
    message.set_configmd5("md5");
    message.set_total_count(2);
    message.add_number(257);
    message.add_number(2);
    message.add_number(3);
    message.add_names("abcd");
    message.add_names("efgh");
    message.add_number2(1000);
    message.add_number2(1);

    unit_test::Exp_GrouData* gd = message.add_grouparray();
    gd->set_name("g1");
    gd->set_md5("aa");
    gd->set_count(-1);
    gd->set_exp_enu(unit_test::E_FINISH);
    gd->add_number3(10);
    gd->add_number3(20);

    gd = message.mutable_ex_group();
    gd->add_number3(20);
    gd->add_number3(30);

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    // raw decode
    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");

    EXPECT_TRUE( pool.BuildFile(file) != nullptr );

    const Descriptor* descriptor = pool.FindMessageTypeByName("EmptyMessage");
    EXPECT_TRUE( descriptor != nullptr );

    DynamicMessageFactory dynamic_factory(&pool);
    std::shared_ptr<Message> msg(dynamic_factory.GetPrototype(descriptor)->New());

    EXPECT_TRUE( msg->ParsePartialFromString(value) );
    EXPECT_TRUE( msg->IsInitialized() );

    // fid 1 already exist and is string, add value as repeated primitive will success
    EXPECT_TRUE( pb_add_int32(msg, "1", 1) );
    // fid 4 already exist and is primitive, add value to it will be failed
    EXPECT_FALSE( pb_add_int32(msg, "4", 1) );
    // fid 1 already exist and is string, add child field will be be failed
    EXPECT_FALSE( pb_add_int32(msg, "1.1", 2) );

    // fid 3 already exist and is repeated embedded_message, and only one element, add value will be failed
    EXPECT_FALSE( pb_add_int32(msg, "3", 2) );
    // fid 8 already exist and is embedded_message, add value will be failed
    EXPECT_FALSE( pb_add_int32(msg, "8", 2) );
    // fid 9 not exist and is enum, add value will success
    EXPECT_TRUE( pb_add_int32(msg, "9", 3) );
    // fid 9 already exist and is enum, add value will be failed
    EXPECT_FALSE( pb_add_int32(msg, "9", 3) );

    // it is repeated embedded_message with only one element, 3.6 is repeated primitive
    EXPECT_TRUE( pb_add_int32(msg, "3.6", 2) );
    // it is repeated embedded_message with only one element, 3.4 is primitive
    EXPECT_FALSE( pb_add_int32(msg, "3.4", 1) );
    // it is repeated embedded_message with only one element, 3.7 is not exist
    EXPECT_TRUE( pb_add_int32(msg, "3.7", 1) );

    EXPECT_TRUE( pb_add_int32(msg, "8.6", 1000) );

    unit_test::Exp_Message_Format msg_format;
    // demo::Exp_Message_Format_Demo msg_format;
    msg_format.set_name("abc");
    msg_format.set_id1(1);
    msg_format.set_id2(1);
    EXPECT_TRUE( msg_format.SerializeToString(&value) );
    EXPECT_TRUE( pb_add_raw_string(msg, "11", value) );

    msg_format.set_id1(2);
    msg_format.set_id2(2);
    EXPECT_TRUE( msg_format.SerializeToString(&value) );
    EXPECT_TRUE( pb_add_raw_string(msg, "11", value) );

    // fprintf(stderr, "%s\n", msg->DebugString().c_str());

    unit_test::Exp_Message message2;
    // EXPECT_TRUE( msg->SerializePartialToString(&value) );
    EXPECT_TRUE( msg->SerializeToString(&value) );
    EXPECT_TRUE( message2.ParseFromString(value) );

    // fprintf(stderr, "%s\n", message2.DebugString().c_str());
}

/**
 * add string:
 * 1. field id is not exist, add it directly, success
 * 2. field id is exist and is primitive, failed
 * 3. field id is exist and is string. success       (repeated string with only one element)     X
 * 4. field id is exist and is repeated string with only one element. success
 * 4. field id is exist and is repeated primitive. success    (same as string)         X
 * 4. field id is exist and is repeated string with more than two elements, success
 * 4. field id is exist and is embedded_message, failed
 * 5. field id is exist and is repeated embedded_message with only one element, failed
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */

 /**
  * add embedded_message string:
  * field id is exist and is embedded_message, success            X
  * field id is exist and is repeated embedded_message with only one element, success
  * field id is exist and is repeated embedded_message with more than two elements, success
  * others failed
  */
TEST(Protobuf3Test, test_pb_add_string)
{

}

/**
 * update primitive:
 * 1. field id is not exist, failed
 * 2. field id is exist and is primitive, success
 * 3. field id is exist and is string. failed
 * 4. field id is exist and is repeated string with only one element. failed
 * 4. field id is exist and is repeated primitive. failed
 * 4. field id is exist and is repeated string with more than two elements, failed
 * 4. field id is exist and is embedded_message, failed
 * 5. field id is exist and is repeated embedded_message with only one element, failed
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */
TEST(Protobuf3Test, test_pb_update_primitive)
{

}

/**
 * update string:
 * 1. field id is not exist, failed
 * 2. field id is exist and is primitive, failed
 * 4. field id is exist and is repeated primitive. success    (same as string)         X
 * 3. field id is exist and is string. success       (repeated string with only one element)
 * 4. field id is exist and is repeated string with only one element. success             X
 * 4. field id is exist and is repeated string with more than two elements, failed
 * 4. field id is exist and is embedded_message, failed
 * 5. field id is exist and is repeated embedded_message with only one element, failed
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */

/**
  * update embedded_message string:
  * field id is exist and is embedded_message, success
  * field id is exist and is repeated embedded_message with only one element, success   X
  * field id is exist and is repeated embedded_message with more than two elements, failed
  * others failed
  */
TEST(Protobuf3Test, test_pb_update_string)
{

}

TEST(Protobuf3Test, test_pb_delete)
{

}

/**
 * read primitive:
 * 1. field id is not exist, failed
 * 2. field id is exist and is primitive, success
 * 3. field id is exist and is string. failed
 * 4. field id is exist and is repeated string with only one element. failed
 * 4. field id is exist and is repeated primitive. failed
 * 4. field id is exist and is repeated string with more than two elements, failed
 * 4. field id is exist and is embedded_message, failed
 * 5. field id is exist and is repeated embedded_message with only one element, failed
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */
TEST(Protobuf3Test, test_pb_read_primitive)
{

}

/**
 * read repeated primitive:
 * 1. field id is not exist, failed
 * 2. field id is exist and is primitive, failed
 * 3. field id is exist and is string. failed
 * 4. field id is exist and is repeated string with only one element. failed
 * 4. field id is exist and is repeated primitive. success
 * 4. field id is exist and is repeated string with more than two elements, failed
 * 4. field id is exist and is embedded_message, failed
 * 5. field id is exist and is repeated embedded_message with only one element, failed
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */
TEST(Protobuf3Test, test_pb_read_repeated_primitive)
{

}

/**
 * read string:
 * 1. field id is not exist, failed
 * 2. field id is exist and is primitive, failed
 * 3. field id is exist and is string. success
 * 4. field id is exist and is repeated string with only one element. success             X
 * 4. field id is exist and is repeated primitive. success
 * 4. field id is exist and is repeated string with more than two elements, failed
 * 4. field id is exist and is embedded_message, success
 * 5. field id is exist and is repeated embedded_message with only one element, success          X
 * 5. field id is exist and is repeated embedded_message with more than two elements, failed
 */
TEST(Protobuf3Test, test_pb_read_string)
{

}

static void pb_show_unknown_field_set(std::shared_ptr<Message> msg)
{
    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    std::unordered_map<int32_t, std::vector<UnknownField*>> child_fields;

    uint32_t field_count = sets->field_count();
    fprintf(stderr, "field_count = %d\n", field_count);

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

    for (auto ele : child_fields) {
        fprintf(stderr, "%d %d\n", ele.first, ele.second.size());
    }
}

static bool pb_sum(std::shared_ptr<Message> msg, const std::vector<uint32_t>& fids, std::string& data)
{
    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());
    UnknownField* target_field = nullptr;

    data.clear();
    std::vector<std::pair<UnknownField*, UnknownFieldSet*>> embedded_childs;
    uint32_t fid_size = fids.size();
    for (int32_t i = 0; i < fid_size; i++) {
        auto cur_fields = parse_unknow_field_sets(sets);
        auto iter = cur_fields.find(fids[i]);
        // INFO: add new field, do not care about type
        if (iter == cur_fields.end()) {
            break;
        }

        auto& vec = iter->second;
        // INFO: it is single primitive
        if (vec.size() == 1 && vec[0]->type() != UnknownField::TYPE_LENGTH_DELIMITED) {
            break;
        }

        // INFO: it is repeated string | repeated embedded message | repeated bytes
        if (vec.size() > 1) {
            break;
        }

        // INFO: it is repeated primitive | single string | embedded message
        UnknownField* field = vec[0];

        UnknownFieldSet* embedded_unknown_field_sets = new UnknownFieldSet();
        const std::string& value = field->length_delimited();
        bool is_embedded_message = (!value.empty() && embedded_unknown_field_sets->ParseFromString(value));

        if (is_embedded_message && i < fid_size - 1) {
            embedded_childs.push_back( std::make_pair(field, embedded_unknown_field_sets) );
            sets = embedded_unknown_field_sets;
        } else if (is_embedded_message && i == fid_size - 1) {
            delete embedded_unknown_field_sets;
            break;
        } else if ( !is_embedded_message && i == fid_size - 1) {
            // wire_format.cc
            // io::CodedInputStream input((const uint8*)value.data(), value.size());
            // while(true) {
            //     uint32 value;
            //     // if (!input.ReadVarint64(&value)) {
            //     //     break;
            //     // }
            //     // fprintf(stderr, "value: %lu\n", WireFormatLite::ZigZagDecode64(value));
            //     if (!input.ReadLittleEndian32(&value)) {
            //         break;
            //     }
            //     //fprintf(stderr, "value: %lu\n", WireFormatLite::ZigZagDecode32(value));
            //     fprintf(stderr, "value: %u\n", value);
            // }
            // repeated primitive
            data.assign(value.data(), value.size());
            // single string
        } else {
            // repeated primitive | single string
            delete embedded_unknown_field_sets;
            /// INFO: it is repeated primitive | single string | repeated string with one element,
            //        and i < fid_size -1, add child field is not allowed
            break;
        }
    }

    // embedded_childs:
    // <root_field, embedded_message>
    //                      ->  <child_field, embedded_message>
    //                                                -> <child_field, embedded_message>
    //                                                                         -> ... ...
    for (auto iter = embedded_childs.rbegin(); iter != embedded_childs.rend(); iter++) {
        delete iter->second;
    }

    if (data.empty()) {
        return false;
    }

    return true;
}

static bool pb_sum_int(std::shared_ptr<Message> msg, const std::vector<uint32_t>& fids, int32_t& value)
{
    std::string data;
    bool ret = pb_sum(msg, fids, data);
    if ( !ret ) {
        return false;
    }

    io::CodedInputStream input((const uint8*)data.data(), data.size());
    while(true) {
        uint32 v;
        if (!input.ReadVarint32(&v)) {
            break;
        }

        fprintf(stderr, "value: %u\n", v);
    }

    return true;
}

TEST(Protobuf3Test, test_pb_list_sum)
{
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(1);
    message.set_id2(2);
    message.set_id3(3);
    message.set_id4(4);
    message.set_id5(5);
    message.set_id6(6);
    message.set_id7(7);
    message.set_id8(8);
    message.set_id9(9);
    message.set_id10(10);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    message.add_name2("name2");
    message.add_name2("name3");

    message.add_id11(10);
    message.add_id11(11);
    message.add_id12(12);
    message.add_id12(13);
    message.add_id13(14);
    message.add_id13(15);
    message.add_id14(14);
    message.add_id14(15);
    message.add_id15(14);
    message.add_id15(15);
    message.add_id16(14);
    message.add_id16(15);
    message.add_id17(14);
    message.add_id17(15);
    message.add_id18(14);
    message.add_id18(15);
    message.add_id19(14);
    message.add_id19(15);
    message.add_id20(14);
    message.add_id20(15);

    message.add_bvalue2(true);
    message.add_bvalue2(true);
    message.add_bv2("123");
    message.add_bv2("123");
    message.add_dv2(1.1);
    message.add_dv2(1.2);

    message.add_fv2(1.1);
    message.add_fv2(2.1);

    unit_test::ExampleApiMessage::EmbeddedMsg* emsg = message.mutable_sub_msg1();
    emsg->set_number(1);
    emsg->set_info("info");

    emsg = message.add_sub_msg2();
    emsg->set_number(1);
    emsg->set_info("info");
    emsg = message.add_sub_msg2();
    emsg->set_number(1);
    emsg->set_info("info");

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    // raw decode
    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");

    EXPECT_TRUE( pool.BuildFile(file) != nullptr );

    const Descriptor* descriptor = pool.FindMessageTypeByName("EmptyMessage");
    EXPECT_TRUE( descriptor != nullptr );

    DynamicMessageFactory dynamic_factory(&pool);
    std::shared_ptr<Message> msg(dynamic_factory.GetPrototype(descriptor)->New());

    EXPECT_TRUE( msg->ParsePartialFromString(value) );
    EXPECT_TRUE( msg->IsInitialized() );

    fprintf(stderr, "%s\n", msg->DebugString().c_str());

    pb_show_unknown_field_set(msg);

    int32_t sum = 0;
    pb_sum_int(msg, {25}, sum);
}

