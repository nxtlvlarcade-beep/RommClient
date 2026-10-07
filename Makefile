CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -Wall -Wextra -std=c99
CPPFLAGS += -Iinclude -Isrc
LDLIBS += -lcurl
CORE_OBJ=src/libromm.o src/minijson.o
CURL_OBJ=src/transport_curl.o
all: libromm.a libromm-curl.a romm-cli
libromm.a: $(CORE_OBJ)
	$(AR) rcs $@ $^
libromm-curl.a: $(CURL_OBJ)
	$(AR) rcs $@ $^
romm-cli: examples/romm_cli.o libromm.a libromm-curl.a
	$(CC) $(CFLAGS) -o $@ examples/romm_cli.o libromm.a libromm-curl.a $(LDLIBS)
clean:
	rm -f $(CORE_OBJ) $(CURL_OBJ) examples/romm_cli.o libromm.a libromm-curl.a romm-cli
.PHONY: all clean
