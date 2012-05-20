TARGET=test

srcs = $(wildcard *Transport.cpp Registry.cpp)
headers = $(wildcard *.h)

all: $(TARGET)

test: $(srcs) $(headers) test.cpp
	clang++ -O2 -g -o $@ $(srcs) test.cpp -lpthread 2>&1
	#clang++ -O2 -g -o $@ $(srcs) -lpthread 2>&1

htest: $(srcs) $(headers) htest.cpp
	clang++ -O2 -g -o $@ $(srcs) htest.cpp -lpthread 2>&1
	#clang++ -O2 -g -o $@ $(srcs) -lpthread 2>&1

.PHONY: run grun
run: $(TARGET)
	./$(TARGET)

grun: $(TARGET)
	gdb ./$(TARGET)
