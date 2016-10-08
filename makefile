CFLAGS = -Wall -Wextra -pedantic -std=gnu99

all:	cs350sh

cs350sh:		cs350sh.c
	gcc $(CFLAGS) -o cs350sh cs350sh.c -g

clean:
	rm -f *.o cs350sh
