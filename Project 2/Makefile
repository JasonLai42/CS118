# server.c and client.c makefile

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -g
SERV_OBJ = server.o segment.o transferlog.o helperfunc.o
CLI_OBJ = client.o segment.o transferlog.o helperfunc.o

submission = 204995126.tar.gz
header_files = constants.h segment.hpp segment.cpp transferlog.hpp transferlog.cpp helperfunc.hpp helperfunc.cpp
submission_files = client.cpp server.cpp $(header_files) Makefile README

default: server client

server: $(SERV_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

client: $(CLI_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

server.o: server.cpp segment.hpp constants.h
	$(CXX) $(CXXFLAGS) -c server.cpp

client.o: client.cpp segment.hpp constants.h
	$(CXX) $(CXXFLAGS) -c client.cpp

segment.o: segment.cpp segment.hpp constants.h
	$(CXX) $(CXXFLAGS) -c segment.cpp

transferlog.o: transferlog.cpp transferlog.hpp constants.h
	$(CXX) $(CXXFLAGS) -c transferlog.cpp

helperfunc.o: helperfunc.cpp helperfunc.hpp
	$(CXX) $(CXXFLAGS) -c helperfunc.cpp

dist:
	@tar -czf $(submission) $(submission_files)

clean:
	@rm -f server client *.o *.file 204995126.tar.gz
