TARGET=test

srcs = $(wildcard *.cpp)
headers = $(wildcard *.h)

$(TARGET): $(srcs) $(headers)
	g++ -O2 -g -o $@ $(srcs) -lpthread 2>&1
	#clang++ -O2 -g -o $@ $(srcs) -lpthread 2>&1

.PHONY: run grun
run: $(TARGET)
	./$(TARGET)

grun: $(TARGET)
	gdb ./$(TARGET)
