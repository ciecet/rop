INC_DIRS = ../../cpp ../../cpp/ed .
HEADER_FILES = $(wildcard $(addsuffix /*.h, $(INC_DIRS)) $(addsuffix /*/*.h, $(INC_DIRS)))
CORE_FILES = $(wildcard $(addprefix ../../cpp, /*.cpp /*.c /ed/*.cpp))
CFLAGS = -ferror-limit=3 -DROP_DEBUG=1 -O0 -g -Wall -lpthread -lcrypto -lrt $(addprefix -I, $(INC_DIRS))

all: serialize

serialize: serialize.cpp $(CORE_FILES) $(HEADER_FILES)
	clang++ -o $@ serialize.cpp $(CORE_FILES) $(CFLAGS)
