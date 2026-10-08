CC = m68k-amigaos-gcc
CFLAGS = -noixemul -m68020 -O0 -g -fno-omit-frame-pointer -Wall -Wextra
BRIDGE = ../../amiga-bridge

.PHONY: all clean
all: sixies_probe

$(BRIDGE)/libbridge.a:
	$(MAKE) -C $(BRIDGE) client

sixies_probe: main.c $(BRIDGE)/libbridge.a
	$(CC) $(CFLAGS) -I$(BRIDGE)/include -o $@ main.c -L$(BRIDGE) -lbridge -lamiga

clean:
	rm -f sixies_probe
