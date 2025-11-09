CXX=g++
CXXFLAGS=-std=gnu++17 -O2 -pthread -Wall -Wextra
LDFLAGS=

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

client: client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f server client *.o
