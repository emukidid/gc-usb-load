ifeq ($(WIN32), 1)
PREFIX = i586-mingw32msvc-
BIN_EXT = .exe
else
PREFIX =
BIN_EXT =
endif

CC = $(PREFIX)gcc
STRIP = $(PREFIX)strip

CFLAGS = -Wall -Wextra -Os -g -pipe
LFLAGS =

BIN = usb-load$(BIN_EXT)

OBJS = \
	gecko.o \
	main.o

all: $(BIN)

clean:
	@rm -f *.o core core.* $(BIN)

strip:
	$(STRIP) -g $(BIN)

install: all
	install -m 755 $(BIN) $(DEVKITPPC)/bin

$(BIN): $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $@

%o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

