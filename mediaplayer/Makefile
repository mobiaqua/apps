PATH := $(CURDIR)/sysroot-native/usr/bin/arm-mobiaqua-linux-gnueabi:$(PATH)
SHELL := env PATH='$(PATH)' /bin/sh
TOOLCHAIN_PREFIX = arm-mobiaqua-linux-gnueabi-
SYSROOT := $(CURDIR)/sysroot

CPP = $(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PREFIX)g++
AR = $(TOOLCHAIN_PREFIX)ar
RANLIB = $(TOOLCHAIN_PREFIX)ranlib
LD = $(TOOLCHAIN_PREFIX)ld
AS = $(TOOLCHAIN_PREFIX)as
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
STRIP = $(TOOLCHAIN_PREFIX)strip

INCS = -I$(SYSROOT)/usr/include/dce -I$(SYSROOT)/usr/include/libdrm -I$(SYSROOT)/usr/include/omap
CXXFLAGS:= -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard -g3 -O0 -std=c++14 -Isrc $(INCS) --sysroot=$(SYSROOT)
LDFLAGS:= -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard --sysroot=$(SYSROOT)

SRCS = $(wildcard src/*.cpp)
ASRCS = $(wildcard src/*.S)
OBJS = $(SRCS:.cpp=.o) $(ASRCS:.S=.o)
DEPS = $(SRCS:.cpp=.d) $(ASRCS:.S=.d)
LIBS = -lavformat -lavcodec -lswscale -lavutil -lz -lpthread -ldrm -ldrm_omap -ldce -lgbm -legl -lGLESv2

all: mediaplayer

mediaplayer: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.S.o:
	$(CXX) $(CXXFLAGS) -x assembler-with-cpp -c $< -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

clean:
	rm -f src/*.o src/*.d

-include $(DEPS)
