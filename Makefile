CFLAGS ?= -O2 -g -std=c89 -pedantic -Wall -Wextra

CFLAGS += $(shell pkg-config --cflags fuse3)
LDLIBS += $(shell pkg-config --libs fuse3)
CPPFLAGS += -DFUSE_USE_VERSION=35

dynlinkfs: dynlinkfs.c
	clang $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

start:
	@doas umount -fl /mnt/dynlinkfs 2>/dev/null; \
	exec doas ./dynlinkfs -f -o allow_other /mnt/dynlinkfs
