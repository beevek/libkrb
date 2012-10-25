LIB = libkrb

CXXFLAGS += -Wall -O2 -fPIC -I..
CXX = g++44
CC = $(CXX)

OBJS =	apache_log_entry.o apache_log_playback.o cached_time.o \
	config_file_parser.o mt_rand.o murmur_hash.o rng_discrete.o

all: $(LIB).a $(LIB).so

libkrb.a: $(LIB).a($(OBJS))

libkrb.so: $(OBJS)
	$(CXX) -shared -Wl,-soname,$@.0 -o $@.0 $(OBJS) -lc
	if [ ! -L $@ ]; then ln -s $@.0 $@ ; fi

clean:
	-rm -rf $(LIB).a $(LIB).so $(LIB).so.0 *.o *~
