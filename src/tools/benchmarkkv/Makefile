SOURCE_PATH=$(shell pwd)
PROJECT_PATH=$(SOURCE_PATH)/../../..
DEPS_PATH=/apps/mdsenv/deps

DEF_INCLUDES = -I$(PROJECT_PATH)/dataservice_bin/include -I$(PROJECT_PATH)/build -I$(PROJECT_PATH)/src/public -I$(DEPS_PATH)/include
DEF_LIBS = $(PROJECT_PATH)/dataservice_bin/lib/libkv_client_api_lib.a $(DEPS_PATH)/lib/libprotobuf.a $(DEPS_PATH)/lib/libevent.a

include $(SOURCE_PATH)/makefile.incl

BENCHMAK_SOURCES = $(SOURCE_PATH)/benchmark.cc $(SOURCE_PATH)/main.cc $(PROJECT_PATH)/build/benchmark.pb.cc

ALL_OBJS    = $(patsubst %.cc, %.o, $(BENCHMAK_SOURCES))

TARGET      = benchmark

all : $(TARGET)

# $(TARGET) : $(ALL_OBJS)
# 	$(AR) $@ $^

$(TARGET) : $(ALL_OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(ALL_OBJS)