TARGET=test

srcs = $(wildcard *.cpp)
headers = $(wildcard *.h)

$(TARGET): $(srcs) $(headers)
	g++ -O2 -g -o $@ $(srcs)

.PHONY: run
run: $(TARGET)
	./$(TARGET)
