ROOTDIR ?= /usr/local
LIBDIR ?= $(ROOTDIR)/lib
INCLUDEDIR ?= $(ROOTDIR)/include

LIB = libkrb

all:
	(cd src && $(MAKE) $@)
	(cd test && $(MAKE) $@)

clean:
	(cd src && $(MAKE) $@)
	(cd test && $(MAKE) $@)

install:
	mkdir -p $(LIBDIR) $(INCLUDEDIR)
	cp src/$(LIB).a src/$(LIB).so.0 src/$(LIB).so $(LIBDIR)
	cp -r krb/ $(INCLUDEDIR)
