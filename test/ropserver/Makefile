BASE_DIR = ../..

SRC_DIRS = cpp cpp/ed remotemap/cpp test/ropserver

HEADERS = $(wildcard $(foreach d, $(SRC_DIRS), $(BASE_DIR)/$(d)/*.h))
SRCS = $(wildcard $(foreach d, $(SRC_DIRS), $(BASE_DIR)/$(d)/*.c $(BASE_DIR)/$(d)/*.cpp))
CFLAGS = -ferror-limit=3 -DROP_DEBUG=1 -O0 -g -Wall -lpthread -lcrypto -lrt $(addprefix -I$(BASE_DIR)/, $(SRC_DIRS))

all: ropserver

test/Test.h: test.idl ../../idl/rop.idl
	../../lib/compile.rb --print-trace test.idl ../../idl/rop.idl
	../../lib/compile.rb -l js test.idl ../../idl/rop.idl

.PHONY: idl
idl: test/Test.h

ropserver: $(SRCS) $(HEADERS) idl
	clang++ -o $@ $(SRCS) $(CFLAGS)
