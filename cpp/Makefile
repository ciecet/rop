TARGET=test

DEBUGFLAGS = -O0 -g
CC = clang $(DEBUGFLAGS)
CXX = clang++ $(DEBUGFLAGS)

srcs = $(wildcard Registry.cpp *Transport.cpp EventDriver.cpp Http.cpp base64.c)
objs = $(patsubst %.cpp, %.o, $(patsubst %.c, %.o, $(srcs)))

headers = $(wildcard *.h)

all: $(TARGET)

%.o: %.cpp $(headers)
	$(CXX) -c $<

%.o: %.c $(headers)
	$(CC) -c $<

test: $(objs) test.o
	$(CXX) -o $@ $(objs) test.o -lpthread -lcrypto

htest: $(objs) htest.o
	$(CXX) -o $@ $(objs) htest.o -lpthread -lcrypto

.PHONY: run grun
run: $(TARGET)
	./$(TARGET)

grun: $(TARGET)
	gdb ./$(TARGET)
