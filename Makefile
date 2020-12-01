CC          = gcc
DEBUG_FLAGS = -ggdb -Wall
CFLAGS      = `pkg-config fuse --cflags --libs` $(DEBUG_FLAGS)

all: mirrorfs caesarfs versfs

mirrorfs: mirrorfs.c
	$(CC) $(CFLAGS) -o mirrorfs mirrorfs.c

caesarfs: caesarfs.c
	$(CC) $(CFLAGS) -o caesarfs caesarfs.c

versfs: versfs.c
	$(CC) $(CFLAGS) -o versfs versfs.c

clean:
	rm -f mirrorfs caesarfs versfs
