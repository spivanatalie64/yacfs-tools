CFLAGS = -Wall -Wextra -O3 -march=native -flto -I src
LDFLAGS = -lzstd -lxxhash -lpthread -flto

OBJS = src/gc.o src/scrub.o src/ctl.o
TARGETS = yacfs-gc yacfs-scrub yacfs-ctl

.PHONY: all clean install

all: $(TARGETS)

yacfs-gc: src/gc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

yacfs-scrub: src/scrub.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

yacfs-ctl: src/ctl.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGETS)

install: $(TARGETS)
	cp $(TARGETS) /usr/local/bin/
