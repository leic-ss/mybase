SOURCE_PATH=$(shell pwd)
PROJECT_PATH=$(SOURCE_PATH)/../../..

DEF_INCLUDES = -I$(PROJECT_PATH)/dataservice_bin/include -I$(PROJECT_PATH)/src/public
DEF_LIBS = $(PROJECT_PATH)/dataservice_bin/lib/libkv_client_api_lib.a $(PROJECT_PATH)/dataservice_bin/lib/libevent.a

include $(SOURCE_PATH)/makefile.incl

BENCHMAK_SOURCES = $(SOURCE_PATH)/kvclientdemo.cc

ALL_OBJS    = $(patsubst %.cc, %.o, $(BENCHMAK_SOURCES))

TARGET      = kvclientdemo

all : $(TARGET)

# $(TARGET) : $(ALL_OBJS)
# 	$(AR) $@ $^

$(TARGET) : $(ALL_OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(ALL_OBJS)
