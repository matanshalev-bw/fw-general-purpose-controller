# Host tool: send arbitrary bluelink packets over USB CDC serial (GPC).
#   make CC=g++ all
#   make CC=aarch64-linux-gnu-g++ all
# Windows (MinGW): make OS=Windows_NT

CC ?= g++

ifeq ($(OS),Windows_NT)
	OUTPUT := gpc_usb_bluelink.exe
	PLATFORM_SRC := win.cpp
else
	ifeq ($(CC),aarch64-linux-gnu-g++)
		OUTPUT := gpc_usb_bluelink_aarch64
	else ifeq ($(CC),g++)
		OUTPUT := gpc_usb_bluelink_x86_64
	else
		$(error Unsupported compiler: $(CC). Use g++ or aarch64-linux-gnu-g++)
	endif
	PLATFORM_SRC := unix.cpp
endif

REPO_ROOT := ../..
LLC_COMMON ?= $(REPO_ROOT)/../fw-llc/scripts/common
BLUELINK_MSG := $(REPO_ROOT)/3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include
BLUELINK_VER := $(REPO_ROOT)/3rd_party/bluelink_sdk/bluelink_version
BLUELINK_SER := $(REPO_ROOT)/3rd_party/bluelink_sdk/bluelink_serializer/bluelink_serializer_include

CFLAGS := -Wall -Wextra -std=c++14
CPPFLAGS := \
	-I$(BLUELINK_MSG) \
	-I$(BLUELINK_MSG)/PayloadStructs \
	-I$(BLUELINK_VER) \
	-I$(BLUELINK_SER) \
	-I$(BLUELINK_SER)/Communicator \
	-I$(BLUELINK_SER)/Communicator/BluelinkCommHandler \
	-I$(BLUELINK_SER)/PacketStruct \
	-I$(BLUELINK_SER)/Serialization/CRC32 \
	-I$(LLC_COMMON)

VPATH := .:$(LLC_COMMON):$(BLUELINK_SER)/Communicator/AckVector:$(BLUELINK_SER)/Communicator:$(BLUELINK_SER)/Serialization/CRC32:$(BLUELINK_SER)/Serialization/Deserializer:$(BLUELINK_SER)/Serialization/Serializer:$(BLUELINK_SER)/Communicator/BluelinkCommHandler

SRC_FILES := \
	main.cpp \
	bluelink_packet_log.cpp \
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

OBJ_DIR := build
OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(SRC_FILES:.cpp=.o))

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(OBJ_FILES)
	$(CC) -o $@ $^

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUTPUT) gpc_usb_bluelink_x86_64 gpc_usb_bluelink_aarch64 gpc_usb_bluelink.exe
