CFLAGS=-Wall -Wextra -Werror -pedantic -gfull
CLIBS=-L. -I.
OUTFILE=ciberia
CC=clang

all: compile

compile:
	$(CC) $(CFLAGS) src/main.c -o $(OUTFILE) $(CLIBS)
run: compile
	./$(OUTFILE) --verbose ./test.cbr
