
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
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message.h>
//#include <json/json.h>

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

namespace pdb_test {
    enum Type {
        TYPE_DOUBLE = 1,    // double, exactly eight bytes on the wire.
        TYPE_FLOAT = 2,     // float, exactly four bytes on the wire.
        TYPE_INT64 = 3,     // int64, varint on the wire.  Negative numbers
                            // take 10 bytes.  Use TYPE_SINT64 if negative
                            // values are likely.
        TYPE_UINT64 = 4,    // uint64, varint on the wire.
        TYPE_INT32 = 5,     // int32, varint on the wire.  Negative numbers
                            // take 10 bytes.  Use TYPE_SINT32 if negative
                            // values are likely.
        TYPE_FIXED64 = 6,   // uint64, exactly eight bytes on the wire.
        TYPE_FIXED32 = 7,   // uint32, exactly four bytes on the wire.
        TYPE_BOOL = 8,      // bool, varint on the wire.
        TYPE_STRING = 9,    // UTF-8 text.
        TYPE_GROUP = 10,    // Tag-delimited message.  Deprecated.
        TYPE_MESSAGE = 11,  // Length-delimited message.

        TYPE_BYTES = 12,     // Arbitrary byte array.
        TYPE_UINT32 = 13,    // uint32, varint on the wire
        TYPE_ENUM = 14,      // Enum, varint on the wire
        TYPE_SFIXED32 = 15,  // int32, exactly four bytes on the wire
        TYPE_SFIXED64 = 16,  // int64, exactly eight bytes on the wire
        TYPE_SINT32 = 17,    // int32, ZigZag-encoded varint on the wire
        TYPE_SINT64 = 18,    // int64, ZigZag-encoded varint on the wire

        MAX_TYPE = 18,  // Constant useful for defining lookup tables
                        // indexed by Type.
    };
}

/*
static void serialize_unknowfieldset(const google::protobuf::UnknownFieldSet& ufs, Json::Value& jnode) {
    std::map<int, std::vector<Json::Value> > kvs;
    for(int i = 0; i < ufs.field_count(); ++i) {
        const auto& uf = ufs.field(i);
        switch(uf.type()) {
            case google::protobuf::UnknownField::TYPE_VARINT:
                kvs[uf.number()].push_back((Json::Int64)uf.varint());
                //jnode[std::to_string(uf.number())] = (Json::Int64)uf.varint();
                break;
            case google::protobuf::UnknownField::TYPE_FIXED32:
                kvs[uf.number()].push_back((Json::UInt)uf.fixed32());
                //jnode[std::to_string(uf.number())] = (Json::Int)uf.fixed32();
                break;
            case google::protobuf::UnknownField::TYPE_FIXED64:
                kvs[uf.number()].push_back((Json::UInt64)uf.fixed64());
                //jnode[std::to_string(uf.number())] = (Json::Int64)uf.fixed64();
                break;
            case google::protobuf::UnknownField::TYPE_LENGTH_DELIMITED:
                google::protobuf::UnknownFieldSet tmp;
                auto& v = uf.length_delimited();
                if(!v.empty() && tmp.ParseFromString(v)) {
                    Json::Value vv;
                    serialize_unknowfieldset(tmp, vv);
                    kvs[uf.number()].push_back(vv);
                    //jnode[std::to_string(uf.number())] = vv;
                } else {
                    //jnode[std::to_string(uf.number())] = v;
                    kvs[uf.number()].push_back(v);
                }
                break;
        }
    }

    for(auto& i : kvs) {
        if(i.second.size() > 1) {
            for(auto& n : i.second) {
                jnode[std::to_string(i.first)].append(n);
            }
        } else {
            jnode[std::to_string(i.first)] = i.second[0];
        }
    }
}

static void serialize_message(const google::protobuf::Message& message, Json::Value& jnode) {
    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();

    for(int i = 0; i < descriptor->field_count(); ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);

        if(field->is_repeated()) {
            if(!reflection->FieldSize(message, field)) {
                continue;
            }
        } else {
            if(!reflection->HasField(message, field)) {
                continue;
            }
        }

        if(field->is_repeated()) {
            switch(field->cpp_type()) {
#define XX(cpptype, method, valuetype, jsontype) \
                case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: { \
                    int size = reflection->FieldSize(message, field); \
                    for(int n = 0; n < size; ++n) { \
                        jnode[field->name()].append((jsontype)reflection->GetRepeated##method(message, field, n)); \
                    } \
                    break; \
                }
            XX(INT32, Int32, int32_t, Json::Int);
            XX(UINT32, UInt32, uint32_t, Json::UInt);
            XX(FLOAT, Float, float, double);
            XX(DOUBLE, Double, double, double);
            XX(BOOL, Bool, bool, bool);
            XX(INT64, Int64, int64_t, Json::Int64);
            XX(UINT64, UInt64, uint64_t, Json::UInt64);
#undef XX
                case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                    int size = reflection->FieldSize(message, field);
                    for(int n = 0; n < size; ++n) {
                        jnode[field->name()].append(reflection->GetRepeatedEnum(message, field, n)->number());
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    int size = reflection->FieldSize(message, field);
                    for(int n = 0; n < size; ++n) {
                        jnode[field->name()].append(reflection->GetRepeatedString(message, field, n));
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                    int size = reflection->FieldSize(message, field);
                    for(int n = 0; n < size; ++n) {
                        Json::Value vv;
                        serialize_message(reflection->GetRepeatedMessage(message, field, n), vv);
                        jnode[field->name()].append(vv);
                    }
                    break;
                }
            }
            continue;
        }

        switch(field->cpp_type()) {
#define XX(cpptype, method, valuetype, jsontype) \
            case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: { \
                jnode[field->name()] = (jsontype)reflection->Get##method(message, field); \
                break; \
            }
            XX(INT32, Int32, int32_t, Json::Int);
            XX(UINT32, UInt32, uint32_t, Json::UInt);
            XX(FLOAT, Float, float, double);
            XX(DOUBLE, Double, double, double);
            XX(BOOL, Bool, bool, bool);
            XX(INT64, Int64, int64_t, Json::Int64);
            XX(UINT64, UInt64, uint64_t, Json::UInt64);
#undef XX
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                jnode[field->name()] = reflection->GetEnum(message, field)->number();
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                jnode[field->name()] = reflection->GetString(message, field);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                serialize_message(reflection->GetMessage(message, field), jnode[field->name()]);
                break;
            }
        }

    }

    const auto& ufs = reflection->GetUnknownFields(message);
    serialize_unknowfieldset(ufs, jnode);
}

static std::string JsonToString(const Json::Value& json) {
    Json::FastWriter w;
    return w.write(json);
}
*/

TEST(ProtobufReflectionTest, refection_test1)
{
    unit_test::ExampleMessage msg_format;
    msg_format.set_type(1);
    msg_format.set_name("aaa");

    unit_test::ExampleMessage_EmbeddedMsg* sub_msg = msg_format.add_submsg();
    sub_msg->set_id(2);
    sub_msg->set_info("bbb");

    sub_msg = msg_format.add_submsg();
    sub_msg->set_id(3);
    sub_msg->set_info("ccc");

    msg_format.add_numbers(1);
    msg_format.add_numbers(2);
    msg_format.add_numbers2(1);
    msg_format.add_numbers2(2);

    std::string value;
    EXPECT_TRUE( msg_format.SerializePartialToString(&value) );

    std::string filename = "../src/proto/unit_test.proto";
    auto pos = filename.find_last_of('/');
    std::string path;
    std::string file;
    if(pos == std::string::npos) {
        file = filename;
    } else {
        path = filename.substr(0, pos);
        file = filename.substr(pos + 1);
    }

    ::google::protobuf::compiler::DiskSourceTree sourceTree;
    sourceTree.MapPath("", path);
    ::google::protobuf::compiler::Importer importer(&sourceTree, NULL);
    importer.Import(file);
    const ::google::protobuf::Descriptor *descriptor = importer.pool()->FindMessageTypeByName("unit_test.ExampleMessage");
    //const ::google::protobuf::Message *message = MessageFactory::generated_factory()->GetPrototype(descriptor);
    EXPECT_TRUE(descriptor != nullptr);

    ::google::protobuf::DynamicMessageFactory factory;
    const ::google::protobuf::Message *message = factory.GetPrototype(descriptor);
    EXPECT_TRUE(message != nullptr);

    ::google::protobuf::Message* msg = message->New();
    EXPECT_TRUE(msg != nullptr);

    EXPECT_TRUE( msg->ParseFromString(value) );
    fprintf(stderr, "%s\n", msg->DebugString().c_str());

    // Json::Value json_value;
    // serialize_message(*msg, json_value);
    // std::cout << JsonToString(json_value) << std::endl;

    delete msg;
}

bool getpbclass(const FileDescriptor* desc, std::string& top_message)
{
    std::unordered_set<std::string> child_messages;
    std::unordered_set<std::string> top_level_messages;
    uint32_t number = desc->message_type_count();
    for (uint32_t i = 0; i<number; i++) {
        fprintf(stderr, "%s %s\n\n", desc->message_type(i)->name().c_str(), desc->message_type(i)->full_name().c_str());
        top_level_messages.emplace(desc->message_type(i)->name());

        const Descriptor* desc2 = desc->message_type(i);
        uint32_t count = desc2->field_count();

        for (uint32_t j = 0; j < count; j++) {
            const FieldDescriptor* fdesc = desc2->field(j);
            if (fdesc->type() == FieldDescriptor::Type::TYPE_MESSAGE) {
                fprintf(stderr, "found message type: %s\n", fdesc->message_type()->full_name().c_str());
                child_messages.emplace(fdesc->message_type()->name());
            }
        }
    }

    for (auto msg : child_messages) {
        fprintf(stderr, "child msg: %s\n", msg.c_str());
        top_level_messages.erase(msg);
    }

    if (top_level_messages.size() == 1) {
        fprintf(stderr, "found pb class %s\n", (top_level_messages.begin() )->c_str());
        top_message = ( top_level_messages.begin() )->c_str();
        return true;
    } else {
        for (auto msg : top_level_messages) {
            fprintf(stderr, "msg: %s\n", msg.c_str());
        }
    }

    return false;
}

TEST(ProtobufReflectionTest, refection_test)
{
    // ::google::protobuf::compiler::MultiFileErrorCollector error_collector;

    std::string filename = "../src/proto/unit_test.proto";
    auto pos = filename.find_last_of('/');
    std::string path;
    std::string file;
    if(pos == std::string::npos) {
        file = filename;
    } else {
        path = filename.substr(0, pos);
        file = filename.substr(pos + 1);
    }

    ::google::protobuf::compiler::DiskSourceTree sourceTree;
    sourceTree.MapPath("", path);
    ::google::protobuf::compiler::Importer importer(&sourceTree, NULL);
    const FileDescriptor * desc = importer.Import(file);

    // ::google::protobuf::compiler::DiskSourceTree sourceTree;
    // sourceTree.MapPath("", "/");
    // ::google::protobuf::compiler::Importer importer(&sourceTree, nullptr);
    // const FileDescriptor * desc = importer.Import("/media/sf_Shares/dataservice/src/proto/unit_test.proto");
    fprintf(stderr, "%s %s %d\n", desc->name().c_str(), desc->package().c_str(), desc->message_type_count());
    std::string pb_class_message;
    if (getpbclass(desc, pb_class_message)) {

    }
}

class SimpleErrorCollector : public io::ErrorCollector {
 public:
  // implements ErrorCollector ---------------------------------------
  void AddError(int line, int column, const std::string& message) {
    last_error_ = std::to_string(line) + ":" + std::to_string(column) + ":" + message;
  }

  const std::string& last_error() { return last_error_; }

 private:
  std::string last_error_;
};

TEST(ProtobufReflectionTest, refection_test2)
{
    // ::google::protobuf::compiler::MultiFileErrorCollector error_collector;
    std::string filename = "../src/proto/config.proto";
    std::string content = FileHelper::readContent(filename);

    DescriptorPool pool;
    io::ArrayInputStream input_stream(content.data(), content.size());
    SimpleErrorCollector error_collector;
    io::Tokenizer tokenizer(&input_stream, &error_collector);
    compiler::Parser parser;
    parser.RecordErrorsTo(&error_collector);
    FileDescriptorProto proto;
    ASSERT_TRUE(parser.Parse(&tokenizer, &proto)) << error_collector.last_error() << "\n" << content;
    ASSERT_EQ("", error_collector.last_error());
    proto.set_name("benchmark.proto");
    const FileDescriptor* desc = pool.BuildFile(proto);
    ASSERT_TRUE(desc != nullptr) << proto.DebugString();
    // EXPECT_EQ(content, desc->DebugString());

    fprintf(stderr, "%s %s %d\n", desc->name().c_str(), desc->package().c_str(), desc->message_type_count());
    std::string pb_class_message;
    if (getpbclass(desc, pb_class_message)) {

    }

    fprintf(stderr, "pb_class_message: %s\n", pb_class_message.c_str());
    const Descriptor* descriptor = desc->FindMessageTypeByName("ConfigHb");
    ASSERT_TRUE(descriptor != nullptr);

    fprintf(stderr, "%s\n", descriptor->DebugString().c_str());

    const FieldDescriptor* field_desc = descriptor->FindFieldByName("sub_msg2");
    fprintf(stderr, "%d %s\n", field_desc->type(), field_desc->is_repeated() ? "true" : "false");
}

TEST(ProtobufReflectionTest, refection_types_test)
{
    std::string filename = "../src/proto/config.proto";
    std::string content = FileHelper::readContent(filename);

    DescriptorPool pool;
    io::ArrayInputStream input_stream(content.data(), content.size());
    SimpleErrorCollector error_collector;
    io::Tokenizer tokenizer(&input_stream, &error_collector);
    compiler::Parser parser;
    parser.RecordErrorsTo(&error_collector);
    FileDescriptorProto proto;
    ASSERT_TRUE(parser.Parse(&tokenizer, &proto)) << error_collector.last_error() << "\n" << content;
    ASSERT_EQ("", error_collector.last_error());
    proto.set_name("config.proto");
    const FileDescriptor* desc = pool.BuildFile(proto);
    ASSERT_TRUE(desc != nullptr) << proto.DebugString();
    // EXPECT_EQ(content, desc->DebugString());

    fprintf(stderr, "%s %s %d\n", desc->name().c_str(), desc->package().c_str(), desc->message_type_count());
    std::string pb_class_message;
    if (getpbclass(desc, pb_class_message)) {

    }

    fprintf(stderr, "pb_class_message: %s\n", pb_class_message.c_str());
    // const Descriptor* descriptor = desc->FindMessageTypeByName(pb_class_message);
    auto descriptor = pool.FindMessageTypeByName("ConfigHb");
    ASSERT_TRUE(descriptor != nullptr);

    fprintf(stderr, "%s\n", descriptor->DebugString().c_str());

    const FieldDescriptor* field_desc1 = descriptor->FindFieldByName("sub_msg2");
    fprintf(stderr, "%s %s %d %s number: %d\n", field_desc1->name().c_str(), field_desc1->full_name().c_str(),
            field_desc1->type(), field_desc1->is_repeated() ? "true" : "false", field_desc1->number());

    const FieldDescriptor* field_desc2 = descriptor->FindFieldByName("name2");
    fprintf(stderr, "%s %s %d %s number: %d\n", field_desc2->name().c_str(), field_desc2->full_name().c_str(),
            field_desc2->type(), field_desc2->is_repeated() ? "true" : "false", field_desc2->number());

    auto desc2 = descriptor->FindFieldByName("sub_msg1");
    const FieldDescriptor* field_desc4 = desc2->message_type()->FindFieldByName("number");
    fprintf(stderr, "%s %s %d %s number: %d\n", field_desc4->name().c_str(), field_desc4->full_name().c_str(),
            field_desc4->type(), field_desc4->is_repeated() ? "true" : "false", field_desc4->number());

    const FieldDescriptor* field_desc3 = descriptor->FindFieldByName("maps");
    fprintf(stderr, "%s %s %d %d %s %d number: %d\n", field_desc3->name().c_str(), field_desc3->full_name().c_str(),
            field_desc3->type(), field_desc3->is_map(), field_desc3->is_repeated() ? "true" : "false", field_desc3->label(), field_desc3->number());
}
