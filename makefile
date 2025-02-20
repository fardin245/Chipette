CFLAGS=-std=c2x
LIBS=C:\chip8\SDL2-2.32.0\x86_64-w64-mingw32\lib -lmingw32 -lSDL2main -lSDL2
INCLUDES=C:\chip8\SDL2-2.32.0\x86_64-w64-mingw32\include

all:
	gcc chip8.c -o chip8 $(CFLAGS) -L$(LIBS) -I$(INCLUDES)