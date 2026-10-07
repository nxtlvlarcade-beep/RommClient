CC ?= cc
AR ?= ar

CFLAGS ?= -O2 -Wall -Wextra -std=c99
CPPFLAGS ?= -Iinclude

LIB_OBJS = \
	src/libromm.o \
	src/minijson.o

CURL_OBJS = \
	src/transport_curl.o

.PHONY: all clean

all: libromm.a libromm-curl.a romm-cli romm-tui

libromm.a: $(LIB_OBJS)
	$(AR) rcs $@ $^

libromm-curl.a: $(CURL_OBJS)
	$(AR) rcs $@ $^

romm-cli: examples/romm_cli.o libromm.a libromm-curl.a
	$(CC) $(CFLAGS) -o $@ \
		examples/romm_cli.o \
		libromm.a libromm-curl.a \
		-lcurl

romm-tui: examples/romm_tui.o libromm.a libromm-curl.a
	$(CC) $(CFLAGS) -o $@ \
		examples/romm_tui.o \
		libromm.a libromm-curl.a \
		-lcurl

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

examples/%.o: examples/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f \
		src/*.o \
		examples/*.o \
		libromm.a \
		libromm-curl.a \
		romm-cli \
		romm-tui
