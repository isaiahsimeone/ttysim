CC=gcc
CFLAGS=-Wall -pedantic -std=c99 -g
DEPS = ttysim.h
OBJ = ttysim.o
LIBS = -pthread

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ttysim: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f *.o
