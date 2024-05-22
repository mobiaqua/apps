PATH := $(CURDIR)/sysroot-native/usr/bin/x86_64-mobiaqua-linux:$(PATH)
SHELL := env PATH='$(PATH)' /bin/sh
TOOLCHAIN_PREFIX = x86_64-mobiaqua-linux-
SYSROOT := $(CURDIR)/sysroot-x86

CPP = $(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PREFIX)g++
AR = $(TOOLCHAIN_PREFIX)ar
RANLIB = $(TOOLCHAIN_PREFIX)ranlib
LD = $(TOOLCHAIN_PREFIX)ld
AS = $(TOOLCHAIN_PREFIX)as
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
STRIP = $(TOOLCHAIN_PREFIX)strip

INCS = -I$(SYSROOT)/usr/include/libdrm
CXXFLAGS:= -march=x86-64-v3 -m64 -g3 -O0 -std=c++14 -Isrc $(INCS) --sysroot=$(SYSROOT)
LDFLAGS:= -sysroot=$(SYSROOT)

SRCS = $(wildcard src/*.cpp)
ASRCS = $(wildcard src/*.S)
OBJS = $(SRCS:.cpp=.o) $(ASRCS:.S=.o)
DEPS = $(SRCS:.cpp=.d) $(ASRCS:.S=.d)
LIBS = -lpthread -ldrm

all: mpv-gui

mpv-gui: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.S.o:
	$(CXX) $(CXXFLAGS) -x assembler-with-cpp -c $< -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

clean:
	rm -f src/*.o src/*.d

-include $(DEPS)
