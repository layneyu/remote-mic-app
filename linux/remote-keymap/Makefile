CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
X11_CFLAGS := $(shell pkg-config --cflags x11 xi xtst)
X11_LIBS := $(shell pkg-config --libs x11 xi xtst)

.PHONY: all test clean

all: xiaomi-keymapd

xiaomi-keymapd: keymapd.c policy.c policy.h runtime_config.c runtime_config.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) keymapd.c policy.c runtime_config.c -o $@ $(X11_LIBS)

test: test_policy test_runtime_config
	./test_policy
	./test_runtime_config

test_policy: policy.c test_policy.c policy.h
	$(CC) $(CFLAGS) policy.c test_policy.c -o $@

test_runtime_config: policy.c runtime_config.c test_runtime_config.c policy.h runtime_config.h
	$(CC) $(CFLAGS) policy.c runtime_config.c test_runtime_config.c -o $@

clean:
	rm -f xiaomi-keymapd test_policy
