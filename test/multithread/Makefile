BASE_DIR = ../..

SRC_DIRS = cpp cpp/ed

HEADERS = $(wildcard $(foreach d, $(SRC_DIRS), $(BASE_DIR)/$(d)/*.h))
SRCS = $(wildcard $(foreach d, $(SRC_DIRS), $(BASE_DIR)/$(d)/*.c $(BASE_DIR)/$(d)/*.cpp))
CFLAGS = -ferror-limit=3 -DROP_DEBUG=1 -O0 -g -Wall -lpthread -lcrypto -lrt $(addprefix -I$(BASE_DIR)/, $(SRC_DIRS)) -I.

all: server client

test/Test.h: test.idl ../../idl/rop.idl
	../../lib/compile.rb --print-trace test.idl ../../idl/rop.idl

.PHONY: idl
idl: test/Test.h

server: server.cpp $(SRCS) $(HEADERS) idl
	clang++ -o $@ server.cpp $(SRCS) $(CFLAGS)

client: client.cpp $(SRCS) $(HEADERS) idl
	clang++ -o $@ client.cpp $(SRCS) $(CFLAGS)
