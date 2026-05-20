# Makefile for the GPC G474 programmer
# make
# make CXX=g++ all
# make CXX=aarch64-linux-gnu-g++ all

CXX ?= g++
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

OBJ_DIR := build

CFLAGS  := -Wall -std=c++14
CPPFLAGS := -I. -I../.. -I../../interfaces/interfaces_g474 \
	-I../../3rd_party/bluelink_sdk/bluelink_version
LDFLAGS := -lstdc++

SRC_FILES := programmer.cpp
OBJ_FILES := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

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
	rm -rf $(OBJ_DIR) g474 g474_*

.PHONY: all run clean
