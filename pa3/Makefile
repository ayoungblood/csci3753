# Makefile
# Loosely based on https://stackoverflow.com/questions/1484817/how-do-i-make-a-simple-makefile-for-gcc-on-linux
TARGET = multi-lookup
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -Wshadow -std=c11 -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes
# -Wall -Wextra -pedantic: stricter warnings
# -Wshadow: warn if a local shadows something else
# -m64: Target x86-64 (don't need)
# -std=c11: ISO/IEC 9899:2011, current C standard
# -save-temps -fverbose-asm -masm=intel: make prettier disassembly (disabled for now)
# -Wpointer-arith: warn on silly pointer operations
# --Wstrict-prototypes -Wmissing-prototypes: be strict about function prototypes
LIBS = -Wall -Wextra
# -Wall -Wextra: stricter linker warnings
# -pthread (not needed on OS X)

.PHONY: test clean
.PRECIOUS: $(TARGET) $(OBJECTS)

# Get all the header files and object files
HEADERS = $(wildcard *.h)
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

# Build all the object files
%.o: %.c $(HEADERS)
		$(CC) $(CFLAGS) -c $< -o $@

# Build the target
$(TARGET): $(OBJECTS)
		$(CC) $(OBJECTS) $(LIBS) -o $@

all: $(TARGET)

test: all
		./multi-lookup input/* output.txt

test-big: all
		./multi-lookup input-big/* output.txt

test-med: all
		./multi-lookup input-med/* output.txt

clean:
		-rm -f *.o
		-rm -f $(TARGET)
		-rm -f output.txt
		-rm -rf multi-lookup.dSYM
