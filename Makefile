CFLAGS = -Wall -Wextra -O3 -flto -I src
LDFLAGS = -lpthread -flto
PREFIX = /usr/local

OBJS = src/gc.o src/scrub.o src/ctl.o src/pool-create.o src/snap-rotate.o
TARGETS = yacfs-gc yacfs-scrub yacfs-ctl yacfs-pool-create yacfs-snap-rotate

.PHONY: all clean install man systemd completion

all: $(TARGETS)

yacfs-gc: src/gc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

yacfs-scrub: src/scrub.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

yacfs-ctl: src/ctl.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

yacfs-pool-create: src/pool-create.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

yacfs-snap-rotate: src/snap-rotate.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c src/yacfs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGETS)

install: all man systemd completion
	cp $(TARGETS) $(PREFIX)/bin/
	mkdir -p $(PREFIX)/share/man/man1
	cp man/yacfs.1 $(PREFIX)/share/man/man1/
	mkdir -p $(PREFIX)/lib/systemd/system
	cp systemd/yacfs@.service $(PREFIX)/lib/systemd/system/
	mkdir -p $(PREFIX)/share/bash-completion/completions
	cp completion/yacfs.bash $(PREFIX)/share/bash-completion/completions/yacfs

man:
	gzip -k -f man/yacfs.1 2>/dev/null || true

systemd:
	@echo "Systemd unit: systemd/yacfs@.service"
	@echo "  Enable with: systemctl enable yacfs@<poolname>"

completion:
	@echo "Bash completion: completion/yacfs.bash"
	@echo "  Source with: source completion/yacfs.bash"
