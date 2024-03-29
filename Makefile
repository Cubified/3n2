all: 3n2

CC=gcc

LIBS=
CFLAGS=-Os -pipe -s $(FEATURES)
DEBUGCFLAGS=-Og -pipe -g -Wall -Wextra $(FEATURES)

INPUT=3n2.c
OUTPUT=3n2

RM=/bin/rm

.PHONY: 3n2
3n2:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(CFLAGS)

debug:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(DEBUGCFLAGS)

clean:
	if [ -e $(OUTPUT) ]; then $(RM) $(OUTPUT); fi
