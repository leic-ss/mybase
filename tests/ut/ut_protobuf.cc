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

#include "config.pb.h"
#include "proto_update_batch.h"

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

static bool repeated_pb_add_raw(UnknownFieldSet* sets, int32_t id, uint64_t item)
{
	std::unordered_map<int32_t, std::vector<UnknownField*>> child_fields = parse_unknow_field_sets(sets);

	auto iter = child_fields.find(id);
	if (iter == child_fields.end()) {
		return false;
	}

	auto& vec = iter->second;
	if (vec.size() == 1) {
		UnknownField* field = vec[0];
		if (field->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
			// proto3 and proto2 packed
			std::string str;
		    io::StringOutputStream output(&str);
		    CodedOutputStream out(&output);
		    out.SetSerializationDeterministic(true);
		    out.WriteVarint64(item);
		    out.Trim();
		    str.append(field->length_delimited().data(), field->length_delimited().size());

		    field->set_length_delimited(str);
		} else if (field->type() != UnknownField::TYPE_GROUP) {
			// proto2 no packed
			sets->AddVarint(id, item);
		}
	} else {
		// proto2 no packed
		sets->AddVarint(id, item);
	}
	return true;
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
static bool repeated_pb_add(std::shared_ptr<Message> msg, const std::string& fields, int32_t item)
{
	const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());
	std::vector<std::string> fids = StringHelper::tokenize(fields, ":");

	// for (uint32_t i = 0; i < sets->field_count(); i++) {
 //        UnknownField* fid = sets->mutable_field(i);
 //        fprintf(stderr, "fid: %d %d\n", fid->number(), fid->type());
 //    }

    std::unordered_map<int32_t, std::vector<UnknownField*>> parent_fields = parse_unknow_field_sets(sets);

	UnknownFieldSet* checking_sets = sets;
	std::vector<std::pair<int32_t, UnknownFieldSet*>> embedded_childs;
	uint32_t size = fids.size();
	for (int32_t i = 0; i < size; i++) {
		int32_t id = std::stoi(fids[i]);
		//fprintf(stderr, "id = %d\n", id);
		if (id < 0) {
			return false;
		}

		if (i == size - 1) {
			if ( !repeated_pb_add_raw(checking_sets, id, item) ) {
				return false;
			}
			break;
		}

		std::unordered_map<int32_t, std::vector<UnknownField*>> child_fields = parse_unknow_field_sets(checking_sets);
		auto iter = child_fields.find(id);
		if (iter == child_fields.end()) {
			return false;
		}

		auto& vec = iter->second;
		// repeated
		if (vec.size() > 1) {
			//fprintf(stderr, "vec.size = %d id = %d\n", vec.size(), id);
			return false;
		}

		// not repeated
		// embedded message
		UnknownField* field = vec[0];
		if (field->type() != UnknownField::TYPE_LENGTH_DELIMITED) {
			// TODO: free memory
			fprintf(stderr, "field->type = %d\n", field->type());
			return false;
		}
		const std::string& value = vec[0]->length_delimited();
		UnknownFieldSet* embedded_unknown_fields = new UnknownFieldSet();
    	if (!value.empty() && embedded_unknown_fields->ParseFromString(value)) {
    		embedded_childs.push_back( std::make_pair(field->number(), embedded_unknown_fields) );
    		checking_sets = embedded_unknown_fields;
    	} else {
    		// TODO: free memory
    		fprintf(stderr, "embedded_unknown_fields parse failed, type[%d] id[%d]\n", field->type(), field->number());
			return false;
    	}
	}

	int32_t child_id = -1;
	std::string embedded_childs_str;
	for (auto iter = embedded_childs.rbegin(); iter != embedded_childs.rend(); iter++) {
		auto id = iter->first;
		auto child_sets = iter->second;

		if (child_id != -1 && !embedded_childs_str.empty()) {
			auto child_fid = child_sets->mutable_field(child_id);
			if ( !child_fid || child_fid->type() != UnknownField::TYPE_LENGTH_DELIMITED ) {
				child_id = -1;
				break;
			}
			child_fid->set_length_delimited(embedded_childs_str);
		}

		child_id = id;
		embedded_childs_str = unknown_field_set_encode(child_sets);

		// uint8_t* d = (uint8_t*)embedded_childs_str.data();
		//    uint32_t size = embedded_childs_str.size();
		//    fprintf(stdout, "TYPE_LENGTH_DELIMITED2[%d]: ", embedded_childs_str.size());
		//    for (uint32_t i = 0; i < size; i++) {
		//        fprintf(stdout, "%02x ", d[i]);
		//    }
		//    fprintf(stdout, "\n");
	}

	if (child_id != -1 && !embedded_childs_str.empty()) {
		auto iter = parent_fields.find(child_id);
		if (iter == parent_fields.end()) {
			return false;
		}

		auto& vec = iter->second;
		// repeated
		if (vec.size() > 1) {
			fprintf(stderr, "vec.size = %d child_id = %d\n", vec.size(), child_id);
			return false;
		}

		auto child_fid = vec[0];
		if ( child_fid && child_fid->type() == UnknownField::TYPE_LENGTH_DELIMITED ) {
			child_fid->set_length_delimited(embedded_childs_str);
		} else {
			fprintf(stderr, "child_id = %d type = %d\n", child_id, child_fid->type());
			return false;
		}
	}

	return true;
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

/**
 
 */
TEST(Protobuf3Test, test_repeated_pb_add)
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

    gd = message.add_grouparray();
    gd->set_name("g2");
    gd->set_md5("bb");
    gd->set_count(2);

    gd = message.mutable_ex_group();
    gd->add_number3(20);
    gd->add_number3(30);

    unit_test::Exp_Message_Format* msg_format = message.mutable_message_format();
	// demo::Exp_Message_Format_Demo msg_format;
    msg_format->set_name("a");
    msg_format->set_id1(1);
    msg_format->set_id2(1);
    msg_format->set_id3(1);
    msg_format->set_id4(1);
    msg_format->set_id5(1);
    msg_format->set_id6(1);
    msg_format->set_id7(1);
    msg_format->set_id8(1);
    msg_format->set_id9(1);
    msg_format->set_id10(1);

    msg_format->set_bvalue(true);
    msg_format->set_bv("bv1");
    msg_format->set_dv1(0.1);
    msg_format->set_fv1(0.1);

    msg_format->add_name2("msg_name");
    msg_format->add_id11(1);
    msg_format->add_id11(1);
    for (int32_t i = 0; i < 2; i++) {
    	msg_format->add_id12(1);
    	msg_format->add_id13(1);
    	msg_format->add_id14(1);
    	msg_format->add_id15(1);
    	msg_format->add_id16(1);
    	msg_format->add_id17(1);
    	msg_format->add_id18(1);
    	msg_format->add_id19(1);
    	msg_format->add_id20(1);

    	msg_format->add_bvalue2(true);
	    msg_format->add_bv2("bv2");
	    msg_format->add_dv2(0.1);
	    msg_format->add_fv2(0.1);
    }

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    // fprintf(stderr, "%s\n", message.DebugString().c_str());

    // uint8_t* d = (uint8_t*)value.data();
    // uint32_t size = value.size();
    // fprintf(stdout, "TYPE_LENGTH_DELIMITED[%d]: ", value.size());
    // for (uint32_t i = 0; i < size; i++) {
    //     fprintf(stdout, "%02x ", d[i]);
    // }
    // fprintf(stdout, "\n");

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

	EXPECT_TRUE( repeated_pb_add(msg, "5", 100) );
	EXPECT_FALSE( repeated_pb_add(msg, "3:6", 100) );
	EXPECT_TRUE( repeated_pb_add(msg, "8:6", 100) );
	EXPECT_TRUE( repeated_pb_add(msg, "11:17", 1000) );

	// fprintf(stderr, "%s\n", msg->DebugString().c_str());

	EXPECT_TRUE( msg->SerializePartialToString(&value) );

	EXPECT_TRUE( message.ParseFromString(value) );

	EXPECT_EQ( message.number_size(), 4 );

	// fprintf(stderr, "%s\n", message.DebugString().c_str());
}

TEST(Protobuf3Test, test_message_format)
{
	unit_test::Exp_Message_Format msg_format;
	// demo::Exp_Message_Format_Demo msg_format;
    msg_format.set_name("abc");
    msg_format.set_id1(1);
    msg_format.set_id2(1);
    msg_format.set_id3(1);
    msg_format.set_id4(1);
    msg_format.set_id5(1);
    msg_format.set_id6(1);
    msg_format.set_id7(1);
    msg_format.set_id8(1);
    msg_format.set_id9(1);
    msg_format.set_id10(1);

    msg_format.set_bvalue(true);
    msg_format.set_bv("bv1");
    msg_format.set_dv1(0.1);
    msg_format.set_fv1(0.1);

    msg_format.add_name2("msg_name");
    msg_format.add_name2("msg_name2");
    for (int32_t i = 0; i < 2; i++) {
        msg_format.add_id11(2);
    	msg_format.add_id12(1);
    	msg_format.add_id13(1);
    	msg_format.add_id14(1);
    	msg_format.add_id15(1);
    	msg_format.add_id16(1);
    	msg_format.add_id17(1);
    	msg_format.add_id18(1);
    	msg_format.add_id19(1);
    	msg_format.add_id20(1);

    	msg_format.add_bvalue2(true);
	    msg_format.add_bv2("bv2");
	    msg_format.add_dv2(0.1);
	    msg_format.add_fv2(0.1);
    }
    msg_format.set_name3("abc");

    std::string value;
    EXPECT_TRUE( msg_format.SerializePartialToString(&value) );

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

	EXPECT_TRUE( repeated_pb_add(msg, "17", 1000) );

	//fprintf(stderr, "%s\n", msg->DebugString().c_str());

	unit_test::Exp_Message_Format msg_format2;
	EXPECT_TRUE( msg->SerializePartialToString(&value) );
	EXPECT_TRUE( msg_format2.ParseFromString(value) );

	// fprintf(stderr, "%s\n", msg_format2.DebugString().c_str());

    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    // for (uint32_t i = 0; i < sets->field_count(); i++) {
    //     UnknownField* fid = sets->mutable_field(i);
    //     fprintf(stderr, "fid[%d] type[%d]\n", fid->number(), fid->type());
    // }

    float fd = 0.1;
    uint32_t fd1 = WireFormatLite::EncodeFloat(fd);
    float fd2 = WireFormatLite::DecodeFloat(fd1);
    fprintf(stderr, "%f\n", fd2);
}

TEST(Protobuf3Test, example_message_format)
{
    unit_test::ExampleMessage msg_format;
    // demo::Exp_Message_Format_Demo msg_format;
    //msg_format.set_type(1);
    //msg_format.set_name("aaa");

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

    fprintf(stderr, "%s\n", msg_format.DebugString().c_str());

    pb_raw_print(value);

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

    const Reflection *reflection = msg->GetReflection();
    UnknownFieldSet* sets = reflection->MutableUnknownFields(msg.get());

    uint32_t field_count = sets->field_count();
    for (uint32_t i = 0; i < field_count; i++) {
        UnknownField* fid = sets->mutable_field(i);        
        int32_t id = fid->number();
        const std::string& value = fid->length_delimited();

        fprintf(stderr, "%d, ", id);
        pb_raw_print(value);
    }
}

TEST(Protobuf4Test, example_message_map)
{
	ConfigHb2 config;

	auto m = config.mutable_maps2();
	(*m)[1] = "a";
	(*m)[2] = "b";

	auto m2 = config.mutable_maps7();
	(*m2)[1] = 100.123;
	(*m2)[2] = 1.3255;

	fprintf(stderr, "%s\n", config.DebugString().c_str());

	std::string value;
    EXPECT_TRUE( config.SerializePartialToString(&value) );
	char *p = StringHelper::convShowString((char*)value.data(), value.size());
    fprintf(stderr, "%s\n", p);

    ConfigHb2 config2;
    {
    	auto m2 = config2.add_msgmap();
	    m2->set_id(1);
	    m2->set_str("a");
    }
    {
    	auto m2 = config2.add_msgmap();
	    m2->set_id(2);
	    m2->set_str("b");
    }
    fprintf(stderr, "%s\n", config2.DebugString().c_str());

    value.clear();
    EXPECT_TRUE( config2.SerializePartialToString(&value) );
	p = StringHelper::convShowString((char*)value.data(), value.size());
    fprintf(stderr, "%s\n", p);
}

TEST(Protobuf5Test, map_put_int_int)
{
	ProtoUpdateBatch batch;
	std::unordered_map<int64_t, int64_t> tmp = {{100, (int64_t)100}, {101, (int64_t)100}};
	batch.map_put(36, tmp);

	std::string input;
	std::string output;
	bool ret = batch.execute(input, output);

	char* p = StringHelper::convShowString((char*)output.data(), output.size());
	fprintf(stderr, "%s\n", p);

	ConfigHb2 config2;
	config2.ParseFromString(output);
	fprintf(stderr, "%s\n", config2.DebugString().c_str());

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<int64_t, int64_t> tmp2 = {{100, (int64_t)101}, {101, (int64_t)101}, {102, (int64_t)102}};
		batch2.map_put(36, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
	}

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<int64_t, int64_t> tmp3 = {{100, (int64_t)101}};
		batch2.map_put(36, tmp3);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
	}
}

TEST(Protobuf5Test, map_put_int_string)
{
	ProtoUpdateBatch batch;
	std::unordered_map<int64_t, int64_t> tmp = {{-100, 100}};
	batch.map_put(36, tmp);
	batch.map_put(37, {{-100, "test 0"}, {-101, "test 1"}});

	std::string input;
	std::string output;
	bool ret = batch.execute(input, output);

	char* p = StringHelper::convShowString((char*)output.data(), output.size());
	fprintf(stderr, "%s\n", p);

	ConfigHb2 config2;
	config2.ParseFromString(output);
	fprintf(stderr, "%s\n", config2.DebugString().c_str());

	{
		ProtoUpdateBatch batch2;
		batch2.map_put(37, {{-100, "test 00"}, {-101, "test 01"}, {-102, "test 02"}});

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}

	{
		ProtoUpdateBatch batch2;
		batch2.map_put(37, {{-100, "test 3"}, {-101, "test 3"}});

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}
}

TEST(Protobuf5Test, map_put_string_int)
{
	ProtoUpdateBatch batch;
	std::unordered_map<std::string, int64_t> tmp = {{"key1", (int64_t)100}, {"key2", (int64_t)200}};
	batch.map_put(38, tmp);
	//batch.map_put(38, "key2", 100);

	std::string input;
	std::string output;
	bool ret = batch.execute(input, output);

	char* p = StringHelper::convShowString((char*)output.data(), output.size());
	fprintf(stderr, "%s\n", p);

	ConfigHb2 config2;
	config2.ParseFromString(output);
	fprintf(stderr, "%s\n", config2.DebugString().c_str());

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<std::string, int64_t> tmp2 = {{"key1", (int64_t)101}, {"key2", (int64_t)202}, {"key3", (int64_t)301}};
		batch2.map_put(38, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<std::string, int64_t> tmp2 = {{"key1", (int64_t)102}, {"key2", (int64_t)203}};
		batch2.map_put(38, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}
}

TEST(Protobuf5Test, map_put_string_string)
{
	ProtoUpdateBatch batch;
	batch.map_put(39, {{"key1", "value1"}, {"key2", "value2"}});
	//batch.map_put(38, "key2", 100);

	std::string input;
	std::string output;
	bool ret = batch.execute(input, output);

	char* p = StringHelper::convShowString((char*)output.data(), output.size());
	fprintf(stderr, "%s\n", p);

	ConfigHb2 config2;
	config2.ParseFromString(output);
	fprintf(stderr, "%s\n", config2.DebugString().c_str());

	{
		ProtoUpdateBatch batch2;
		batch2.map_put(39, {{"key1", "value 1"}, {"key2", "value 2"}, {"key3", "value 3"}});

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}

	{
		ProtoUpdateBatch batch2;
		batch2.map_put(39, {{"key1", "value 11"}, {"key2", "value 22"}});

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}
}

TEST(Protobuf5Test, map_put_int_double)
{
	ProtoUpdateBatch batch;
	std::unordered_map<int64_t, double> tmp = {{100, 1.111}, {200, 2.222}};
	batch.map_put(40, tmp);
	//batch.map_put(38, "key2", 100);

	std::string input;
	std::string output;
	bool ret = batch.execute(input, output);

	char* p = StringHelper::convShowString((char*)output.data(), output.size());
	fprintf(stderr, "%s\n", p);

	ConfigHb2 config2;
	config2.ParseFromString(output);
	fprintf(stderr, "%s\n", config2.DebugString().c_str());

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<int64_t, double> tmp2 = {{100, 3.333}, {300, 3.333}, {200, 3.333}};
		batch2.map_put(40, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<int64_t, double> tmp2 = {{300, 4.441}, {400, 4.441}};
		batch2.map_put(40, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}
}

TEST(Protobuf5Test, map_put_string_double)
{
	ProtoUpdateBatch batch;
	std::unordered_map<std::string, double> tmp = {{"100", 1.111}, {"200", 2.222}};
	batch.map_put(41, tmp);
	//batch.map_put(38, "key2", 100);

	std::string input;
	std::string output;
	bool ret = batch.execute(input, output);

	char* p = StringHelper::convShowString((char*)output.data(), output.size());
	fprintf(stderr, "%s\n", p);

	ConfigHb2 config2;
	config2.ParseFromString(output);
	fprintf(stderr, "%s\n", config2.DebugString().c_str());

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<std::string, double> tmp2 = {{"100", 3.333}, {"300", 3.333}, {"200", 3.333}};
		batch2.map_put(41, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}

	{
		ProtoUpdateBatch batch2;
		std::unordered_map<std::string, double> tmp2 = {{"300", 4.441}, {"400", 4.441}};
		batch2.map_put(41, tmp2);

		std::string output2;
		ret = batch2.execute(output, output2);
		p = StringHelper::convShowString((char*)output2.data(), output2.size());
		fprintf(stderr, "%s\n", p);

		config2.ParseFromString(output2);
		fprintf(stderr, "%s\n", config2.DebugString().c_str());
		output = output2;
	}
}
// pb_add_int32()