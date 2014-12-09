# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

CC        = gcc
STRIP     = strip
AS        = as

PREFIX    = /usr
OBJS      = ../main.o ../cpu.o ../memory.o ../video.o ../input.o ../sound.o \
		 ../cpu_threaded.o ../gui.o x86_stub.o ../cheats.o ../zip.o

# Platform specific definitions 

ifeq ($(shell uname -a),)
EXE_EXT = .exe
else ifneq ($(findstring MINGW,$(shell uname -a)),)
EXE_EXT = .exe
else
EXE_EXT =
endif

BIN       ?= gpsp$(EXE_EXT)

CFLAGS     += -DPC_BUILD -Wall -m32
INCLUDES   = -I$(PREFIX)/include `sdl-config --cflags`
LIBS       = -L$(PREFIX)/lib32 `sdl-config --libs` -lz -m32

# Compilation:

ifeq ($(DEBUG), 1)
OPTIMIZE	      := -O0 -g
OPTIMIZE_SAFE  := -O0 -g
else
OPTIMIZE	      := -O3
OPTIMIZE_SAFE  := -O2
endif

all:	$(BIN)

%.o: %.c
	$(CC) $(CFLAGS) $(OPTIMIZE) $(INCLUDES) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(OPTIMIZE) -c -o $@ $<

../cpu.o: ../cpu.c
	$(CC) -c -o $@ $< $(CFLAGS) -Wno-unused-variable -Wno-unused-label $(OPTIMIZE_SAFE) $(INCLUDES)

../cpu_threaded.o: ../cpu_threaded.c
	$(CC) -c -o $@ $< $(CFLAGS) -Wno-unused-variable -Wno-unused-label $(OPTIMIZE_SAFE) $(INCLUDES)


$(BIN):	$(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $(BIN)

clean:
	rm -f $(OBJS) $(BIN)

