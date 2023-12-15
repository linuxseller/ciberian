CFLAGS=-Wall -Wextra -Werror -pedantic
CLIBS=-L. -I.
OUTFILE=ciberia
CC=tcc

all: compile

compile:
	mkdir -p build
	$(CC) $(CFLAGS) main.c -o $(OUTFILE) $(CLIBS)
run: compile
	./$(OUTFILE) --verbose ./test.cbr
