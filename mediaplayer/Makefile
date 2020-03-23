
ifndef OE_BASE
$(error OE_BASE is not set)
endif

HOST_TYPE := $(shell uname -s)

ifeq ($(HOST_TYPE),Linux)
HOST_SUFFIX = x86_64-linux
endif

ifeq ($(HOST_TYPE),Darwin)
HOST_SUFFIX = x86_64-darwin
endif

PATH := $(OE_BASE)/build-mobiaqua-tv/tmp/sysroots/$(HOST_SUFFIX)/usr/armv7a-hf/bin:$(PATH)

TOOLCHAIN_PREFIX = arm-linux-gnueabi-

CPP = $(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PREFIX)g++
AR = $(TOOLCHAIN_PREFIX)ar
RANLIB = $(TOOLCHAIN_PREFIX)ranlib
LD = $(TOOLCHAIN_PREFIX)ld
AS = $(TOOLCHAIN_PREFIX)as
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
STRIP = $(TOOLCHAIN_PREFIX)strip

CXXFLAGS:= -g3 -O0 -std=c++14 -Isrc
LDFLAGS:=

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)
LIBS = -lavformat -lavcodec -lswscale -lavutil -lz -lpthread -ldrm -ldrm_omap -ldce -lgbm -legl

all: mediaplayer

mediaplayer: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $*.o

clean:
	rm -f src/*.o src/*.d

-include $(DEPS)
