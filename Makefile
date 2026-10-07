CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99
CPPFLAGS += -Iinclude
LDLIBS += -lcurl

OBJ = src/libromm.o src/transport_curl.o

all: romm-cli

romm-cli: $(OBJ) examples/romm_cli.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(OBJ) examples/romm_cli.o romm-cli

.PHONY: all clean
