TARGET=test

srcs = $(wildcard *.cpp)
headers = $(wildcard *.h)

$(TARGET): $(srcs)
	g++ -g -o $@ $^

$(srcs):: $(headers)

.PHONY: run
run: $(TARGET)
	./$(TARGET)
