CFLAGS += -Wall -Wextra
CFLAGS += `pkg-config fuse3 --cflags --libs`

all:	fbemu-fuse

fbemu-fuse:
	$(CC) $(CFLAGS) fbemu-fuse.c -o fbemu-fuse

clean:
	rm -f fbemu-fuse

