#CXX=g++-14
CXX=clang++
SANFLAGS=-fsanitize=address -fsanitize=undefined
#SANFLAGS=-fsanitize=thread
#SANFLAGS= 
DBGFLAGS=-g $(SANFLAGS)
RELFLAGS=-O3 -march=native
RWDFLAGS=$(RELFLAGS) $(DBGFLAGS)

override CFLAGS :=$(CFLAGS) -std=c++17 -pipe -Wall -Wextra -Wno-deprecated-declarations
override LDFLAGS :=$(LDFLAGS) -lgtest -pthread

OBJ_LIST=./src/map/ut.o ./src/map/bench.o ./src/queue/ut.o ./src/queue/verification.o ./src/queue/bench.o

all: build
	
build: test

%.o: %.cpp
	$(CXX) -c -o $@ $^  $(CFLAGS) $(TARGETFLAGS) -I ./src



dbg: TARGETFLAGS = $(DBGFLAGS)
dbg: $(OBJ_LIST)
	$(CXX) -o test ./src/main.cpp $(OBJ_LIST) $(CFLAGS) $(TARGETFLAGS) $(LDFLAGS)

rwd: TARGETFLAGS = $(RWDFLAGS)
rwd: $(OBJ_LIST)
	$(CXX) -o test ./src/main.cpp $(OBJ_LIST) $(CFLAGS) $(TARGETFLAGS) $(LDFLAGS)

rls: TARGETFLAGS = $(RELFLAGS)
rls: $(OBJ_LIST)
	$(CXX) -o test ./src/main.cpp $(OBJ_LIST) $(CFLAGS) $(TARGETFLAGS) $(LDFLAGS)

clean:
	find . -type f -name '*.o' -exec rm {} +
	rm test || true

format:
	find ./src/ -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' -exec clang-format --style=file -i {} \;

.PHONY: test
