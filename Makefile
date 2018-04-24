TARGET = test

CFLAGS = -g -Wall
LIBRARY = -lm


$(TARGET): test.c multiarm.c
	gcc $(CFLAGS) -o $@ $^ $(LIBRARY)
