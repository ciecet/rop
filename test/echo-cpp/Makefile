INC_DIRS = ../../cpp ../../cpp/ed .
HEADER_FILES = $(wildcard $(addsuffix /*.h, $(INC_DIRS)) $(addsuffix /*/*.h, $(INC_DIRS)))
CORE_FILES = $(wildcard $(addprefix ../../cpp, /*.cpp /*.c /ed/*.cpp))
CFLAGS = -ferror-limit=3 -DROP_DEBUG=1 -O0 -g -Wall -lpthread -lcrypto -lrt $(addprefix -I, $(INC_DIRS))
CFLAGS += -DED_MANGLE_PREFIX=ALTI_

all: echotest

test/Echo.h: echo.idl
	../../lib/compile.rb --print-trace echo.idl

.PHONY: idl
idl: test/Echo.h

echotest: echotest.cpp $(CORE_FILES) $(HEADER_FILES) idl
	clang++ -o $@ echotest.cpp $(CORE_FILES) $(CFLAGS)

echoserver: echoserver.cpp $(CORE_FILES) $(HEADER_FILES) idl
	clang++ -o $@ echoserver.cpp $(CORE_FILES) $(CFLAGS)

echoclient: echoclient.cpp $(CORE_FILES) $(HEADER_FILES) idl
	clang++ -o $@ echoclient.cpp $(CORE_FILES) $(CFLAGS)
