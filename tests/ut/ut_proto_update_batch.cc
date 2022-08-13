#include "unit_test.pb.h"
#include "demo.pb.h"
#include "common.h"
#include "proto_update_batch.h"

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

using WireType = internal::WireFormatLite::WireType;
using WireFormatLite = internal::WireFormatLite;

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

static std::string generate_example_message() {
    unit_test::ExampleApiMessage message;
    message.set_name("name1");
    message.set_id1(8);
    message.set_id2(8);
    message.set_id3(8);
    message.set_id4(8);
    message.set_id5(8);
    message.set_id6(8);
    message.set_id7(8);
    message.set_id8(8);
    message.set_id9(8);
    message.set_id10(8);

    message.set_bvalue(false);
    message.set_bv("bv info");
    message.set_dv1(1.1);
    message.set_fv1(2.1);

    for (uint32 i = 0; i < 10; i++) {
        unit_test::ExampleApiMessage::EmbeddedMsg* emsg = message.add_sub_msg2();
        emsg->set_number(i);
        emsg->set_info("info1");
        emsg->set_number4(i);
        emsg->set_number5(i);
        emsg->set_number6(i);
        emsg->set_number7(i);
        emsg->set_number8(i);
        emsg->set_number9(i);
        emsg->set_number10(i);
    }
    
    std::string value;
    message.SerializeToString(&value);
    return std::move(value);
}

static unit_test::ExampleApiMessage parse_example_message(const std::string& value)
{
    unit_test::ExampleApiMessage message;
    message.ParseFromString(value);
    return message;
}

// TODO: if bool is false, need add field
TEST(Protobuf3UpdateBatch, test_pb_update)
{
    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.update_field(2, 100);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(3, 100);

        // uint32 100 / 4294967295
        batch.update_field(4, 100);
        // uint64 100 / 18446744073709551615
        batch.update_field(5, 100);

        // fixed32 100 / 4294967295
        batch.update_field(8, 100);
        // fixed64 100 / 18446744073709551615
        batch.update_field(9, 100);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.update_field(10, 100);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(11, 100);

        batch.update_field(12, 1000);

        batch.update_field(13, "test 2");

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), 100 );
        EXPECT_EQ( message.id2(), 100 );
        EXPECT_EQ( message.id3(), 100 );
        EXPECT_EQ( message.id4(), 100 );
        EXPECT_EQ( message.id7(), 100 );
        EXPECT_EQ( message.id8(), 100 );
        EXPECT_EQ( message.id9(), 100 );
        EXPECT_EQ( message.id10(), 100 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.update_field(2, 4294967295);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(3, 18446744073709551615);

        // uint32 100 / 4294967295
        batch.update_field(4, 4294967295);
        // uint64 100 / 18446744073709551615
        batch.update_field(5, 18446744073709551615);

        // fixed32 100 / 4294967295
        batch.update_field(8, 4294967295);
        // fixed64 100 / 18446744073709551615
        batch.update_field(9, 18446744073709551615);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.update_field(10, 4294967295);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(11, 18446744073709551615);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), -1 );
        EXPECT_EQ( message.id2(), -1 );
        EXPECT_EQ( message.id3(), 4294967295 );
        EXPECT_EQ( message.id4(), 18446744073709551615 );
        EXPECT_EQ( message.id7(), 4294967295 );
        EXPECT_EQ( message.id8(), 18446744073709551615 );
        EXPECT_EQ( message.id9(), -1 );
        EXPECT_EQ( message.id10(), -1 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.update_field(2, -1);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(3, -1);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.update_field(10, -1);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(11, -1);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), -1 );
        EXPECT_EQ( message.id2(), -1 );
        EXPECT_EQ( message.id9(), -1 );
        EXPECT_EQ( message.id10(), -1 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.update_field(2, -100);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(3, -100);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.update_field(10, -100);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.update_field(11, -100);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), -100 );
        EXPECT_EQ( message.id2(), -100 );
        EXPECT_EQ( message.id9(), -100 );
        EXPECT_EQ( message.id10(), -100 );
    }
}

TEST(Protobuf3UpdateBatch, test_pb_incr)
{
    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.incr_field(2, 100);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(3, 100);

        // uint32 100 / 4294967295
        batch.incr_field(4, 100);
        // uint64 100 / 18446744073709551615
        batch.incr_field(5, 100);

        // fixed32 100 / 4294967295
        batch.incr_field(8, 100);
        // fixed64 100 / 18446744073709551615
        batch.incr_field(9, 100);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.incr_field(10, 100);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(11, 100);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), 108 );
        EXPECT_EQ( message.id2(), 108 );
        EXPECT_EQ( message.id3(), 108 );
        EXPECT_EQ( message.id4(), 108 );
        EXPECT_EQ( message.id7(), 108 );
        EXPECT_EQ( message.id8(), 108 );
        EXPECT_EQ( message.id9(), 108 );
        EXPECT_EQ( message.id10(), 108 );
    }

    {
        std::string value;
        ProtoUpdateBatch batch;
        batch.incr_field(2, 1000);
        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);
        EXPECT_EQ( message.id1(), 1000 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.incr_field(2, 4294967287);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(3, 18446744073709551607);

        // uint32 100 / 4294967295 / -1
        batch.incr_field(4, 4294967287);
        // uint64 100 / 18446744073709551615 / -1
        batch.incr_field(5, 18446744073709551607);

        // fixed32 100 / 4294967295 / -1
        batch.incr_field(8, 4294967287);
        // fixed64 100 / 18446744073709551615 / -1
        batch.incr_field(9, 18446744073709551607);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.incr_field(10, 4294967287);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(11, 18446744073709551607);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), -1 );
        EXPECT_EQ( message.id2(), -1 );
        EXPECT_EQ( message.id3(), 4294967295 );
        EXPECT_EQ( message.id4(), 18446744073709551615 );
        EXPECT_EQ( message.id7(), 4294967295 );
        EXPECT_EQ( message.id8(), 18446744073709551615 );
        EXPECT_EQ( message.id9(), -1 );
        EXPECT_EQ( message.id10(), -1 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.incr_field(2, -1);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(3, -1);

        // uint32 100 / 4294967295 / -1
        batch.incr_field(4, -1);
        // uint64 100 / 18446744073709551615 / -1
        batch.incr_field(5, -1);

        // fixed32 100 / 4294967295 / -1
        batch.incr_field(8, -1);
        // fixed64 100 / 18446744073709551615 / -1
        batch.incr_field(9, -1);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.incr_field(10, -1);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(11, -1);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), 7 );
        EXPECT_EQ( message.id2(), 7 );
        EXPECT_EQ( message.id3(), 7 );
        EXPECT_EQ( message.id4(), 7 );
        EXPECT_EQ( message.id7(), 7 );
        EXPECT_EQ( message.id8(), 7 );
        EXPECT_EQ( message.id9(), 7 );
        EXPECT_EQ( message.id10(), 7 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        // int32 100 / 4294967295 / -1 / -100
        batch.incr_field(2, -100);
        // int64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(3, -100);

        // uint32 100 / 4294967295 / -1
        batch.incr_field(4, -100);
        // uint64 100 / 18446744073709551615 / -1
        batch.incr_field(5, -100);

        // fixed32 100 / 4294967295 / -1
        batch.incr_field(8, -100);
        // fixed64 100 / 18446744073709551615 / -1
        batch.incr_field(9, -100);

        // sfixed32 100 / 4294967295 / -1 / -100
        batch.incr_field(10, -100);
        // sfixed64 100 / 18446744073709551615 / -1 / -100
        batch.incr_field(11, -100);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_STREQ( message.name().c_str(), "tests 1");
        EXPECT_EQ( message.id1(), -92 );
        EXPECT_EQ( message.id2(), -92 );
        EXPECT_EQ( message.id3(), -92 );
        EXPECT_EQ( message.id4(), -92 );
        EXPECT_EQ( message.id7(), -92 );
        EXPECT_EQ( message.id8(), -92 );
        EXPECT_EQ( message.id9(), -92 );
        EXPECT_EQ( message.id10(), -92 );
    }
}

TEST(Protobuf3UpdateBatch, test_list_add_message_by_int)
{
    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(512);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, true, true, false, 10);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 10 );
    }

    {
        std::string value = generate_example_message();

        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(512);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, true, true, false, 1, false);

        //batch.list_add_message_by_int(33, value2, 1, 1, true, true, false, 1);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message2 = parse_example_message(output);

        EXPECT_EQ( message2.sub_msg2().size(), 1 );
        EXPECT_EQ( message2.sub_msg2()[0].number(), 512 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(512);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, true, false, false, 1, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 1 );
        EXPECT_EQ( message.sub_msg2()[0].number(), 0 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(0);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, true, true, false, 11, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 10 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(0);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, false, true, false, 11, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 11 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(0);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, false, true, true, 11, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 10 );
    }

    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number(0);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 1, 1, false, true, true, 11, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 10 );
    }

    // uint32
    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number5(100);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 5, 5, true, false, false, 1, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 1 );
        EXPECT_EQ( message.sub_msg2()[0].number5(), 0 );
    }

    // uint32
    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number5(100);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 5, 5, true, true, false, 1, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 1 );
        EXPECT_EQ( message.sub_msg2()[0].number5(), 100 );
    }
}

TEST(Protobuf3UpdateBatch, test_list_add_message_by_int_benchmark)
{
    for (uint32 i = 0; i < 100; i++)
    {
        std::string value = generate_example_message();
        ProtoUpdateBatch batch;
        batch.update_field(1, "tests 1");

        unit_test::ExampleApiMessage::EmbeddedMsg emsg;
        emsg.set_number5(100);
        emsg.set_info("info3");
        std::string value2;
        emsg.SerializeToString(&value2);
        batch.list_add_message_by_int(32, value2, 5, 5, true, true, false, 1, false);

        std::string output;
        EXPECT_TRUE( batch.execute(value, output) );
        unit_test::ExampleApiMessage message = parse_example_message(output);

        EXPECT_EQ( message.sub_msg2().size(), 1 );
        EXPECT_EQ( message.sub_msg2()[0].number5(), 100 );
    }
}

TEST(Protobuf3UpdateBatch, test_update_repeated_int)
{
    unit_test::ExampleApiMessage message;
    message.add_id11(1);
    message.add_id11(2);

    std::string value;
    message.SerializeToString(&value);

    fprintf(stderr, "%s\n", message.DebugString().c_str());
    pb_raw_print(value);

    ProtoUpdateBatch batch;
    batch.update_field(17, "aaaa");

    std::string output;
    EXPECT_TRUE( batch.execute(value, output) );
    pb_raw_print(output);
}

TEST(Protobuf3UpdateBatch, test_list_add_raw_bytes_not_exist)
{
    unit_test::ExampleApiMessage message;
    message.add_id11(1);
    message.add_id11(2);

    std::string value;
    message.SerializeToString(&value);

    ProtoUpdateBatch batch;
    batch.list_add_raw_bytes(16, "aaaa");

    unit_test::ExampleApiMessage::EmbeddedMsg emsg;
    emsg.set_number5(100);
    emsg.set_info("info3");
    std::string value2;
    emsg.SerializeToString(&value2);
    batch.list_add_message_by_int(32, value2, 5, 5, true, true, false, 1, false);

    std::string output;
    EXPECT_TRUE( batch.execute(value, output) );
    unit_test::ExampleApiMessage message2;
    message2.ParseFromString(output);
    fprintf(stderr, "%s\n", message2.DebugString().c_str());
}