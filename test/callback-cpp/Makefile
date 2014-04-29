INC_DIRS = ../../cpp ../../cpp/ed . 
HEADER_FILES = $(wildcard $(addsuffix /*.h, $(INC_DIRS)) $(addsuffix /*/*.h, $(INC_DIRS)))
CORE_FILES = $(wildcard $(addprefix ../../cpp, /*.cpp /*.c /ed/*.cpp))
CFLAGS = -ferror-limit=3 -DROP_DEBUG=1 -O0 -g -Wall -lpthread -lcrypto -lrt $(addprefix -I, $(INC_DIRS))
CFLAGS += -DED_MANGLE_PREFIX=ALTI_

all: server

test/Test.h: test.idl
	../../lib/compile.rb --print-trace test.idl

.PHONY: idl
idl: test/Test.h

server: server.cpp $(CORE_FILES) $(HEADER_FILES) idl
	clang++ -o $@ server.cpp $(CORE_FILES) $(CFLAGS)
