# Makefile for the GPC G474 programmer
# make
# make CXX=g++ all
# make CXX=aarch64-linux-gnu-g++ all
# Windows (MinGW): make OS=Windows_NT

CXX ?= g++

ifeq ($(OS),Windows_NT)
	OUTPUT := g474.exe
	PLATFORM_SRC := win.cpp
	EXTRA_LDFLAGS :=
else
	ifeq ($(CXX),aarch64-linux-gnu-g++)
		ifeq ($(shell which $(CXX) 2>/dev/null),)
			$(warning WARNING: aarch64-linux-gnu-g++ not found, falling back to g++)
			CXX := g++
			OUTPUT := g474
		else
			OUTPUT := g474_aarch64
		endif
	else ifeq ($(CXX),g++)
		OUTPUT := g474_x86_64
	else
		OUTPUT := g474
	endif
	PLATFORM_SRC := unix.cpp
	EXTRA_LDFLAGS := -lpthread
endif

REPO_ROOT := ../..
BLUELINK_MSG := $(REPO_ROOT)/3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include
BLUELINK_VER := $(REPO_ROOT)/3rd_party/bluelink_sdk/bluelink_version
BLUELINK_SER := $(REPO_ROOT)/3rd_party/bluelink_sdk/bluelink_serializer/bluelink_serializer_include
LLC_COMMON ?= $(REPO_ROOT)/../fw-llc/scripts/common

OBJ_DIR := build

CFLAGS  := -Wall -Wextra -std=c++14
CPPFLAGS := \
	-I. \
	-I$(REPO_ROOT) \
	-I$(BLUELINK_MSG) \
	-I$(BLUELINK_MSG)/PayloadStructs \
	-I$(BLUELINK_VER) \
	-I$(BLUELINK_SER) \
	-I$(BLUELINK_SER)/Communicator \
	-I$(BLUELINK_SER)/Communicator/BluelinkCommHandler \
	-I$(BLUELINK_SER)/PacketStruct \
	-I$(BLUELINK_SER)/Serialization/CRC32 \
	-I$(LLC_COMMON) \
	-I$(REPO_ROOT)/interfaces/interfaces_g474

LDFLAGS := -lstdc++ $(EXTRA_LDFLAGS)

VPATH := .:$(LLC_COMMON):$(BLUELINK_SER)/Communicator/AckVector:$(BLUELINK_SER)/Communicator:$(BLUELINK_SER)/Serialization/CRC32:$(BLUELINK_SER)/Serialization/Deserializer:$(BLUELINK_SER)/Serialization/Serializer:$(BLUELINK_SER)/Communicator/BluelinkCommHandler

SRC_FILES := \
	programmer.cpp \
	usb_transport.cpp \
	concrete_bluelink_callbacks.cpp \
	serial.cpp \
	host_platform.cpp \
	$(PLATFORM_SRC) \
	ack_vector.cpp \
	bluelink_callbacks.cpp \
	checksum_32.cpp \
	deserializer.cpp \
	serializer.cpp \
	bluelink_communication_handler.cpp

ifneq ($(OS),Windows_NT)
SRC_FILES += can_transport.cpp
endif

OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(SRC_FILES:.cpp=.o)))

all: $(OBJ_DIR) $(OUTPUT)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OUTPUT): $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(OUTPUT)
	./$(OUTPUT)

clean:
	rm -rf $(OBJ_DIR) g474 g474_* g474.exe

.PHONY: all run clean
