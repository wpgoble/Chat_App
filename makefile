CC=g++
CXXFLAGS=-std=c++11 -Wall -pedantic -pthread

.SUFFIXES: .x

.PHONY: clean

chat: chat_client.x chat_server.x

clean:
	rm -f *.o *.x core.*

%.x: %.cpp
	$(CC) $(CXXFLAGS) -o $@ $<
