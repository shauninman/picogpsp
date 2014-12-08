# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

CC        = gcc
STRIP     = strip
AS        = as

PREFIX    = /usr
OBJS      = main.o cpu.o memory.o video.o input.o sound.o \
	    cpu_threaded.o gui.o x86_stub.o cheats.o zip.o

# Platform specific definitions 

ifeq ($(shell uname -a),)
EXE_EXT = .exe
else ifneq ($(findstring MINGW,$(shell uname -a)),)
EXE_EXT = .exe
else
EXE_EXT =
endif

BIN       ?= gpsp$(EXE_EXT)

VPATH      += ..
CFLAGS     += -DPC_BUILD -Wall -m32
INCLUDES   = -I${PREFIX}/include `sdl-config --cflags`
LIBS       = -L${PREFIX}/lib32 `sdl-config --libs` -lz -m32

# Compilation:


.SUFFIXES: .c .S

all:	${BIN}

%.o: %.c
	${CC} ${CFLAGS} ${INCLUDES} -c -o $@ $<

%.o: %.S
	${CC} ${CFLAGS} -c -o $@ $<

cpu.o cpu_threaded.o: CFLAGS += -Wno-unused-variable -Wno-unused-label

${BIN}:	${OBJS}
	${CC} ${OBJS} ${LIBS} -o ${BIN}  
#	${STRIP} ${BIN}

clean:
	rm -f *.o ${BIN} 

