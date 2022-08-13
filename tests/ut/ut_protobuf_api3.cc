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

union fdata_ {
    uint64 varint_;
    uint32 fixed32_;
    uint64 fixed64_;
    mutable std::string* length_delimited_;
};

static bool pb_update_val(const std::string& idata, int32_t ftag, fdata_ val, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

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

static bool pb_incr_val(const std::string& idata, int32_t ftag, fdata_ val, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

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
          if (ftag == tag) {
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

static bool pb_parse_list_bytes(const std::string& idata, int32_t ftag, std::vector<std::string*>& bytes, CodedOutputStream& out)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    // io::StringOutputStream output(&odata);
    // CodedOutputStream out(&output);
    // out.SetSerializationDeterministic(true);

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
            bytes.push_back(str);
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

    out.Trim();
    return true;
}

static bool pb_list_add_message(const std::string& idata, int32_t ftag, std::string* val, int32_t ptag, std::string& odata)
{
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    std::vector<std::string*> sstr;
    bool rc = pb_parse_list_bytes(idata, ftag, sstr, out);

    if (ptag & 7 != WireType::WIRETYPE_LENGTH_DELIMITED) {
        std::map<uint64_t, std::string*> ssmap;
        fdata_ data_;
        data_.varint_ = 0;
        parse_message(ptag, *val, data_);
        ssmap.emplace(data_.varint_, val);

        for (auto pstr : sstr) {
            data_.varint_ = 0;
            parse_message(ptag, *pstr, data_);
            ssmap.emplace(data_.varint_, pstr);
        }

        for (auto& item : ssmap) {
            out.WriteVarint32(WireFormatLite::MakeTag(ftag >> 3, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
            out.WriteVarint32(item.second->size());
            out.WriteRawMaybeAliased(item.second->data(), item.second->size());
        }
    } else {
        std::map<std::string, std::string*> ssmap;
        fdata_ data_;
        data_.length_delimited_ = nullptr;
        parse_message(ptag, *val, data_);
        if (data_.length_delimited_) {
            ssmap.emplace(*data_.length_delimited_, val);
        }

        for (auto pstr : sstr) {
            data_.length_delimited_ = nullptr;
            parse_message(ptag, *pstr, data_);
            if (data_.length_delimited_) {
                ssmap.emplace(*data_.length_delimited_, pstr);
            }
        }

        for (auto& item : ssmap) {
            out.WriteVarint32(WireFormatLite::MakeTag(ftag >> 3, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
            out.WriteVarint32(item.second->size());
            out.WriteRawMaybeAliased(item.second->data(), item.second->size());
        }
    }

    for (auto pstr : sstr) {
        delete pstr;
    }

    out.Trim();
    return true;
}

static bool pb_list_add_primitive(const std::string& idata, int32_t ftag, int32_t ptag, int64_t val, std::string& odata)
{
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    std::vector<std::string*> sstr;
    bool rc = pb_parse_list_bytes(idata, ftag, sstr, out);
    if (sstr.size() != 1) {
        // TODO: relase memory
        return false;
    }

    if (ptag & 7 == WireType::WIRETYPE_LENGTH_DELIMITED) {
        // TODO: release memory
        return false;
    }

    std::string tmp;
    io::StringOutputStream otmp(&tmp);
    CodedOutputStream outtmp(&otmp);
    outtmp.SetSerializationDeterministic(true);

    switch (ptag & 7) {
    case WireType::WIRETYPE_VARINT: {
        outtmp.WriteVarint64(val);
        break;
    }
    case WireType::WIRETYPE_FIXED64: {
        outtmp.WriteLittleEndian64(val);
        break;
    }
    case WireType::WIRETYPE_FIXED32: {
        outtmp.WriteLittleEndian32(val);
        break;
    }
    default:
        break;
    }
    outtmp.Trim();

    uint32 size = sstr[0]->size() + tmp.size();
    out.WriteVarint32(ftag);
    out.WriteVarint32(size);
    out.WriteRawMaybeAliased(sstr[0]->data(), sstr[0]->size());
    out.WriteRawMaybeAliased(tmp.data(), tmp.size());

    delete sstr[0];

    out.Trim();
    return true;
}

bool pb_update_int(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.varint_ = value;
    return pb_update_val(input, (number << 3) | WireType::WIRETYPE_VARINT, val, output);
}

bool pb_update_sint(std::string& input, int32_t number, int64_t value, std::string& output)
{
    uint64_t encoded_val = WireFormatLite::ZigZagEncode64(value);
    return pb_update_int(input, number, encoded_val, output);
}

bool pb_update_string(std::string& input, int32_t number, std::string& value, std::string& output)
{
    fdata_ val;
    val.length_delimited_ = &value;
    return pb_update_val(input, (number << 3) | WireType::WIRETYPE_LENGTH_DELIMITED, val, output);
}

bool pb_update_fixed32(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed32_ = value;
    return pb_update_val(input, (number << 3) | WireType::WIRETYPE_FIXED32, val, output);
}

bool pb_update_fixed64(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed64_ = value;
    return pb_update_val(input, (number << 3) | WireType::WIRETYPE_FIXED64, val, output);
}

static bool pb_update_float(std::string& input, int32_t number, float val, std::string& output)
{
    uint32_t encoded_val = WireFormatLite::EncodeFloat(val);
    return pb_update_fixed32(input, number, encoded_val, output);
}

static bool pb_update_double(std::string& input, int32_t number, double val, std::string& output)
{
    uint64_t encoded_val = WireFormatLite::EncodeDouble(val);
    return pb_update_fixed64(input, number, encoded_val, output);
}

static bool pb_incr_int(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.varint_ = value;
    return pb_incr_val(input, (number << 3) | WireType::WIRETYPE_VARINT, val, output);
}

static bool pb_incr_fixed32(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed32_ = value;
    return pb_incr_val(input, (number << 3) | WireType::WIRETYPE_FIXED32, val, output);
}

static bool pb_incr_fixed64(std::string& input, int32_t number, int64_t value, std::string& output)
{
    fdata_ val;
    val.fixed64_ = value;
    return pb_incr_val(input, (number << 3) | WireType::WIRETYPE_FIXED64, val, output);
}

struct PipeLineCmd {
    int32_t cmd; // 1 - update, 2 - incr, 3 - list_add_message, 4 - list_add_primitive
    int32_t ftag;
    fdata_ val;
    int32_t ptag;
};

static bool pb_pipe_line(std::string& idata, PipeLineCmd* pipe_line_cmd, std::string& odata)
{
    io::CodedInputStream input((const uint8*)idata.data(), idata.size());
    io::StringOutputStream output(&odata);
    CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);

    std::unordered_map<int32_t, std::vector<std::string*>> smap;

    while(true) {
        if (input.ConsumedEntireMessage()) {
            break;
        }

        uint32 tag;
        if (!input.ReadVarint32(&tag)) {
            break;
        }

        int32_t number = tag >> 3;
        if (number >= 192) {
            return false;
        }

        switch (tag & 7) {
        case WireType::WIRETYPE_VARINT: {
            uint64 value;
            bool rc = input.ReadVarint64(&value);
            if (pipe_line_cmd[number].cmd == 0) {
                // do nothing
            } else if (pipe_line_cmd[number].cmd == 1 && pipe_line_cmd[number].ftag == tag) { // update
                value = pipe_line_cmd[number].val.varint_;
            } else if (pipe_line_cmd[number].cmd == 2 && pipe_line_cmd[number].ftag == tag) {  // incr
                value += (int64_t)pipe_line_cmd[number].val.varint_;
            }
            out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_VARINT));
            out.WriteVarint64(value);
            break;
        }
        case WireType::WIRETYPE_FIXED64: {
            uint64 value;
            bool rc = input.ReadLittleEndian64(&value);
            if (pipe_line_cmd[number].cmd == 0) {
                // do nothing
            } else if (pipe_line_cmd[number].cmd == 1 && pipe_line_cmd[number].ftag == tag) { // update
                value = pipe_line_cmd[number].val.fixed64_;
            } else if (pipe_line_cmd[number].cmd == 2 && pipe_line_cmd[number].ftag == tag) {  // incr
                value += (int64_t)pipe_line_cmd[number].val.fixed64_;
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
            // if (pipe_line_cmd[number].cmd != 0) {
            //     fprintf(stderr, "%d %d\n", pipe_line_cmd[number].cmd, pipe_line_cmd[number].ftag >> 3);
            // }

            std::string* str = new std::string();
            bool rc = input.ReadString(str, size);
            if (pipe_line_cmd[number].cmd == 0) {
                out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
                out.WriteVarint32(str->size());
                out.WriteRawMaybeAliased(str->data(), str->size());
                delete str;
            } else if (pipe_line_cmd[number].cmd == 1 && pipe_line_cmd[number].ftag == tag) { // update
                out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
                out.WriteVarint32(pipe_line_cmd[number].val.length_delimited_->size());
                out.WriteRawMaybeAliased(pipe_line_cmd[number].val.length_delimited_->data(), pipe_line_cmd[number].val.length_delimited_->size());
            } else if ( (pipe_line_cmd[number].cmd == 3 || pipe_line_cmd[number].cmd == 4 || pipe_line_cmd[number].cmd == 5) &&
                        pipe_line_cmd[number].ftag == tag ) {
                //list_add_message | list_add_string | list_add_primitive
                auto iter = smap.find(number);
                if (iter == smap.end()) {
                    smap.emplace(number, std::vector<std::string*>());
                    iter = smap.find(number);
                }
                std::vector<std::string*>& str_vec = iter->second;
                str_vec.push_back(str);
            }

            break;
        }
        case WireType::WIRETYPE_FIXED32: {
            uint32 value;
            bool rc = input.ReadLittleEndian32(&value);
            if (pipe_line_cmd[number].cmd == 0) {
                // do nothing
            } else if (pipe_line_cmd[number].cmd == 1 && pipe_line_cmd[number].ftag == tag) { // update
                value = pipe_line_cmd[number].val.fixed32_;
            } else if (pipe_line_cmd[number].cmd == 2 && pipe_line_cmd[number].ftag == tag) {  // incr
                value += (int32_t)pipe_line_cmd[number].val.fixed32_;
            }

            out.WriteVarint32(WireFormatLite::MakeTag(number, WireFormatLite::WIRETYPE_FIXED32));
            out.WriteLittleEndian32(value);
            break;
        }
        default:
          break;
      }
    }

    for (auto& item : smap) {
        switch(pipe_line_cmd[item.first].cmd) {
        case 3: {  //list_add_message
            std::map<uint64_t, std::string*> ssmap;
            fdata_ data_;
            data_.varint_ = 0;
            parse_message(pipe_line_cmd[item.first].ptag, *pipe_line_cmd[item.first].val.length_delimited_, data_);
            ssmap.emplace(data_.varint_, pipe_line_cmd[item.first].val.length_delimited_);

            std::vector<std::string*>& str_vec = item.second;
            for (auto& pstr : str_vec) {
                data_.varint_ = 0;
                parse_message(pipe_line_cmd[item.first].ptag, *pstr, data_);
                ssmap.emplace(data_.varint_, pstr);
            }
            for (auto& item2 : ssmap) {
                out.WriteVarint32(WireFormatLite::MakeTag(item.first, WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
                out.WriteVarint32(item2.second->size());
                out.WriteRawMaybeAliased(item2.second->data(), item2.second->size());
            }

            for (auto& pstr : str_vec) {
                delete pstr;
            }
        }
        case 4: {

        }
        case 5: {

        }
        break;
        }
    }

    out.Trim();
    return true;
}

// static bool pb_list_add_message_by_fixed32(std::string& input, int32_t fnumber, std::string* value, int32_t pnumber, std::string& output)
// {
//     fdata_ val;
//     val.length_delimited_ = value;
//     int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
//     int32_t ptag = pnumber << 3 | WireType::WIRETYPE_FIXED32;

//     return pb_list_add_bytes(input, ftag, val, ptag, output);
// }

static bool pb_list_add_message_by_fixed32(std::string& input, int32_t fnumber, std::string* value, int32_t pnumber, std::string& output)
{
    int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    int32_t ptag = pnumber << 3 | WireType::WIRETYPE_FIXED32;
    return pb_list_add_message(input, ftag, value, ptag, output);
}

static bool pb_list_add_message_by_bytes(std::string& input, int32_t fnumber, std::string* value, int32_t pnumber, std::string& output)
{
    int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    int32_t ptag = pnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    return pb_list_add_message(input, ftag, value, ptag, output);
}

static bool pb_list_add_int(std::string& input, int32_t fnumber, int64_t value, std::string& output)
{
    int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    int32_t ptag = 0x0 << 3 | WireType::WIRETYPE_VARINT;
    return pb_list_add_primitive(input, ftag, ptag, value, output);
}

static bool pb_list_add_sint(std::string& input, int32_t fnumber, int32_t value, std::string& output)
{
    uint32_t encoded_val = WireFormatLite::ZigZagEncode32(value);
    return pb_list_add_int(input, fnumber, encoded_val, output);
}

static bool pb_list_add_sint(std::string& input, int32_t fnumber, int64_t value, std::string& output)
{
    uint64_t encoded_val = WireFormatLite::ZigZagEncode64(value);
    return pb_list_add_int(input, fnumber, encoded_val, output);
}

static bool pb_list_add_fixed32(std::string& input, int32_t fnumber, int64_t value, std::string& output)
{
    int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    int32_t ptag = 0x0 << 3 | WireType::WIRETYPE_FIXED32;
    return pb_list_add_primitive(input, ftag, ptag, value, output);
}

static bool pb_list_add_fixed64(std::string& input, int32_t fnumber, int64_t value, std::string& output)
{
    int32_t ftag = fnumber << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
    int32_t ptag = 0x0 << 3 | WireType::WIRETYPE_FIXED64;
    return pb_list_add_primitive(input, ftag, ptag, value, output);
}

TEST(Protobuf3Api3Test, test_pb_update)
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

    for (uint32 i = 0; i < 2; i++) {
        unit_test::ExampleApiMessage::EmbeddedMsg* emsg = message.add_sub_msg2();
        emsg->set_number(i);
        emsg->set_info("info1");
    }

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );

    //fprintf( stderr, "%s\n", message.DebugString().c_str() );

    std::string output;
    EXPECT_TRUE( pb_update_int(value, 3, 33, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_int(value, 4, 4294967295, output) );
    value.assign(output); output.clear();

    EXPECT_TRUE( pb_update_sint(value, 6, -10000, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_sint(value, 7, -10000000, output) );
    value.assign(output); output.clear();

    std::string val("test");
    EXPECT_TRUE( pb_update_string(value, 1, val, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_string(value, 8, val, output) );
    value.assign(output); output.clear();

    EXPECT_TRUE( pb_update_fixed32(value, 8, 4294967295, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_fixed32(value, 10, 4294967295, output) );
    value.assign(output); output.clear();

    EXPECT_TRUE( pb_update_fixed64(value, 9, 18446744073709551615, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_fixed64(value, 11, 18446744073709551615, output) );
    value.assign(output); output.clear();

    EXPECT_TRUE( pb_update_double(value, 14, 3.111111, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_float(value, 15, 5.111111, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_float(value, 14, 3.111111, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_update_double(value, 15, 5.111111, output) );
    value.assign(output); output.clear();

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );

    //fprintf( stderr, "%s\n", message2.DebugString().c_str() );
}

static void pb_raw_print(const std::string& str)
{
    uint8_t* ch = (uint8_t*)str.data();
    uint32_t size = str.size();

    for (uint32_t i = 0; i < size; i++) {
        fprintf(stdout, "%02x ", ch[i]);
    }
    fprintf(stdout, "\n");
}

TEST(Protobuf3Api3Test, test_pb_incr)
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
    EXPECT_TRUE( pb_incr_int(value, 2, 4294967293, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_incr_int(value, 3, -1, output) );
    value.assign(output); output.clear();
    EXPECT_TRUE( pb_incr_int(value, 4, -4, output) );
    value.assign(output); output.clear();
    // EXPECT_TRUE( pb_incr_int(msg, 3, 18446744073709551612) );
    // EXPECT_TRUE( pb_incr_int(msg, 4, 4294967291) );
    // EXPECT_TRUE( pb_incr_int(msg, 5, 18446744073709551610) );

    pb_incr_fixed32(value, 8, -1, output);
    value.assign(output); output.clear();
    pb_incr_fixed32(value, 10, -1, output);
    value.assign(output); output.clear();
    pb_incr_fixed64(value, 9, -1, output);
    value.assign(output); output.clear();
    pb_incr_fixed64(value, 11, -1, output);

    // EXPECT_TRUE( pb_incr_float(msg, 15, 1.222) );
    // EXPECT_TRUE( pb_incr_double(msg, 14, 2.1111) );

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );

    //fprintf( stderr, "%s\n", message2.DebugString().c_str() );
    //fprintf( stderr, "666 %lu\n", TimeHelper::currentUs() );
}

TEST(Protobuf3Api3Test, test_pb_pipeline)
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

    std::string value;
    EXPECT_TRUE( message.SerializeToString(&value) );
    fprintf( stderr, "%s\n", message.DebugString().c_str() );

    std::string output;
    PipeLineCmd pipe_line_cmd[192] = {0};
    pipe_line_cmd[2].cmd = 1;
    pipe_line_cmd[2].ftag = 0x2 << 3 | WireType::WIRETYPE_VARINT;
    pipe_line_cmd[2].val.varint_ = 4294967295;

    pipe_line_cmd[3].cmd = 1;
    pipe_line_cmd[3].ftag = 0x3 << 3 | WireType::WIRETYPE_VARINT;
    pipe_line_cmd[3].val.varint_ = 4294967295;

    pb_pipe_line(value, pipe_line_cmd, output);

    // EXPECT_TRUE( pb_incr_float(msg, 15, 1.222) );
    // EXPECT_TRUE( pb_incr_double(msg, 14, 2.1111) );

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );

    fprintf( stderr, "%s\n", message2.DebugString().c_str() );
    //fprintf( stderr, "666 %lu\n", TimeHelper::currentUs() );
}

TEST(Protobuf3Api3Test, test_pb_list_add)
{
    //fprintf( stderr, "111 %lu\n", TimeHelper::currentUs() );
    unit_test::ExampleApiMessage message;

    message.add_id11(100);
    message.add_id11(101);
    message.add_id15(-100);
    message.add_id15(-101);
    message.add_id17(1000);
    message.add_id17(1010);
    for (uint32 i = 0; i < 2; i++) {
        unit_test::ExampleApiMessage::EmbeddedMsg* emsg = message.add_sub_msg2();
        emsg->set_number(i);
        emsg->set_info("info1");
    }

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
    pb_list_add_message_by_fixed32(value, 32, value2, 1, output);

    unit_test::ExampleApiMessage message2;
    EXPECT_TRUE( message2.ParseFromString(output) );
    pb_raw_print(output);
    fprintf( stderr, "111 %s\n", message2.DebugString().c_str() );

    output.clear();
    pb_list_add_message_by_bytes(value, 32, value2, 2, output);

    EXPECT_TRUE( message2.ParseFromString(output) );
    pb_raw_print(output);
    fprintf( stderr, "222 %s\n", message2.DebugString().c_str() );

    output.clear();
    pb_list_add_int(value, 17, 103, output);
    std::string tmp = output; output.clear();
    pb_list_add_int(tmp, 17, 4294967295, output);

    EXPECT_TRUE( message2.ParseFromString(output) );
    pb_raw_print(output);
    fprintf( stderr, "333 %s\n", message2.DebugString().c_str() );

    output.clear();
    pb_list_add_sint(value, 21, (int32_t)4294967295, output);

    EXPECT_TRUE( message2.ParseFromString(output) );
    pb_raw_print(output);
    fprintf( stderr, "444 %s\n", message2.DebugString().c_str() );

    output.clear();
    pb_list_add_fixed32(value, 23, (int32_t)4294967295, output);

    EXPECT_TRUE( message2.ParseFromString(output) );
    pb_raw_print(output);
    fprintf( stderr, "555 %s\n", message2.DebugString().c_str() );
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
TEST(Protobuf3Api3Test, test_pb_benchmark)
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
    fprintf(stderr, "value.size: %d\n", value.size());

    uint32_t total_times = 100;

    {
        uint64_t start_ts = TimeHelper::currentUs();

        for (uint32_t i = 0; i < total_times; i++) {
            unit_test::ExampleApiMessage message2;
            EXPECT_TRUE( message2.ParseFromString(value) );

            message2.set_id9(20);
            message2.set_id10(21);
            std::string value2;
            EXPECT_TRUE( message.SerializeToString(&value2) );
        }

        fprintf( stderr, "ts1: %lu message: \n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();
        unit_test::ExampleApiMessage message2;

        for (uint32_t i = 0; i < total_times; i++) {
            std::string output;
            pb_update_int(value, 2, 100, output);
        }

        fprintf( stderr, "ts2: %lu message2: \n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();
        for (uint32 i = 0; i < total_times; i++) {
            std::string output;
            PipeLineCmd pipe_line_cmd[192] = {0};
            pipe_line_cmd[2].cmd = 1;
            pipe_line_cmd[2].ftag = 0x2 << 3 | WireType::WIRETYPE_VARINT;
            pipe_line_cmd[2].val.varint_ = 4294967295;

            pipe_line_cmd[3].cmd = 1;
            pipe_line_cmd[3].ftag = 0x3 << 3 | WireType::WIRETYPE_VARINT;
            pipe_line_cmd[3].val.varint_ = 4294967295;

            pb_pipe_line(value, pipe_line_cmd, output);
        }

        fprintf( stderr, "ts3: %lu\n", TimeHelper::currentUs() - start_ts );
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
        fprintf( stderr, "ts4: %lu\n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();

        for (uint32_t i = 0; i < total_times; i++) {
            unit_test::ExampleApiMessage::EmbeddedMsg emsg2;
            emsg2.set_number(512);
            emsg2.set_info("info3");
            std::string* value2 = new std::string();
            emsg2.SerializeToString(value2);

            std::string output;
            pb_list_add_message_by_bytes(value, 32, value2, 1, output);
            delete value2;
        }

        fprintf( stderr, "ts5: %lu\n", TimeHelper::currentUs() - start_ts );
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();

        std::string output;
        for (uint32 i = 0; i < total_times; i++) {
            output.clear();
            PipeLineCmd* pipe_line_cmd = (PipeLineCmd*)calloc(1024, sizeof(PipeLineCmd));
            pipe_line_cmd[2].cmd = 1;
            pipe_line_cmd[2].ftag = 0x2 << 3 | WireType::WIRETYPE_VARINT;
            pipe_line_cmd[2].val.varint_ = 4294967295;

            pipe_line_cmd[3].cmd = 1;
            pipe_line_cmd[3].ftag = 0x3 << 3 | WireType::WIRETYPE_VARINT;
            pipe_line_cmd[3].val.varint_ = 4294967295;

            unit_test::ExampleApiMessage::EmbeddedMsg emsg2;
            emsg2.set_number(512);
            emsg2.set_info("info3");
            std::string* value2 = new std::string();
            emsg2.SerializeToString(value2);

            pipe_line_cmd[32].cmd = 3;
            pipe_line_cmd[32].ftag = 32 << 3 | WireType::WIRETYPE_LENGTH_DELIMITED;
            pipe_line_cmd[32].val.length_delimited_ = value2;
            pipe_line_cmd[32].ptag = 1 << 3 | WireType::WIRETYPE_FIXED32;

            pb_pipe_line(value, pipe_line_cmd, output);
        }

        unit_test::ExampleApiMessage message2;
        EXPECT_TRUE( message2.ParseFromString(output) );
        // fprintf( stderr, "ts6: %lu message: %s\n", TimeHelper::currentUs() - start_ts,  message2.DebugString().c_str());
        fprintf( stderr, "ts6: %lu\n", TimeHelper::currentUs() - start_ts);
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();

        std::string output;
        for (uint32 i = 0; i < total_times; i++) {
            output.clear();
            ProtoUpdateBatch batch;

            batch.update_field(2, 4294967295);
            batch.update_field(3, 4294967295);
            batch.execute(value, output);
        }

        // unit_test::ExampleApiMessage message2;
        // EXPECT_TRUE( message2.ParseFromString(output) );
        //fprintf( stderr, "ts7: %lu message: %s\n", TimeHelper::currentUs() - start_ts,  message2.DebugString().c_str());
        fprintf( stderr, "ts7: %lu\n", TimeHelper::currentUs() - start_ts);
    }

    {
        uint64_t start_ts = TimeHelper::currentUs();

        std::string output;
        for (uint32 i = 0; i < total_times; i++) {
            output.clear();
            ProtoUpdateBatch batch;

            unit_test::ExampleApiMessage::EmbeddedMsg emsg2;
            emsg2.set_number(512);
            emsg2.set_info("info3");
            std::string value2;
            emsg2.SerializeToString(&value2);

            batch.update_field(2, 4294967295);
            batch.update_field(3, 4294967295);
            batch.list_add_message_by_int(32, value2, 1, 1, true, true, false, 100);
            batch.execute(value, output);
        }

        //unit_test::ExampleApiMessage message2;
        //EXPECT_TRUE( message2.ParseFromString(output) );
        //fprintf( stderr, "ts8: %lu message: %s\n", TimeHelper::currentUs() - start_ts,  message2.DebugString().c_str());
        fprintf( stderr, "ts8: %lu\n", TimeHelper::currentUs() - start_ts);
    }
}
