
CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -Wall -Wextra -std=c99
CPPFLAGS += -Iinclude -Isrc $(shell $(PKG_CONFIG) --cflags libcurl)
LDLIBS += $(shell $(PKG_CONFIG) --libs libcurl)

CORE_OBJ = src/libromm.o src/minijson.o
CURL_OBJ = src/transport_curl.o

.PHONY: all clean rebuild

all: libromm.a libromm-curl.a romm-cli romm-tui

libromm.a: $(CORE_OBJ)
	$(AR) rcs $@ $^

libromm-curl.a: $(CURL_OBJ)
	$(AR) rcs $@ $^

romm-cli: examples/romm_cli.o libromm.a libromm-curl.a
	$(CC) $(CFLAGS) -o $@ examples/romm_cli.o \
		libromm.a libromm-curl.a $(LDLIBS)

romm-tui: examples/romm_tui.o libromm.a libromm-curl.a
	$(CC) $(CFLAGS) -o $@ examples/romm_tui.o \
		libromm.a libromm-curl.a $(LDLIBS)

rebuild:
	$(MAKE) clean
	$(MAKE) all

clean:
	rm -f $(CORE_OBJ) $(CURL_OBJ)
	rm -f examples/romm_cli.o examples/romm_tui.o
	rm -f libromm.a libromm-curl.a
	rm -f romm-cli romm-tui
