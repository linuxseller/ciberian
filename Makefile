CFLAGS=-Wall -Wextra -Werror -pedantic -ggdb3
CLIBS=-L. -I.
OUTFILE=ciberia
CC=clang

all: compile

compile:
	$(CC) $(CFLAGS) main.c -o $(OUTFILE) $(CLIBS)
run: compile
	./$(OUTFILE) --verbose ./test.cbr
