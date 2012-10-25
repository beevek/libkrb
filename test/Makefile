CXXFLAGS += -Wall -g3 -fPIC -I..
CXX = g++44
CC = $(CXX)
LDFLAGS = -L../src -lkrb -lpthread

PROGS = bloom tobloom wss apachelog time sumbytes lossyhash rpool tpool cparse lctrie
CXXFLAGS = -Wall -O3 -fPIC -I..

all: $(PROGS)

tpool: LDFLAGS += -levent

cparse: LDFLAGS += -lboost_program_options-mt

lctrie: LDFLAGS += -lboost_serialization-mt -lboost_iostreams-mt

clean:
	-rm -rf $(PROGS) *.o
