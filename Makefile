CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
TARGET = test

all: $(TARGET)

$(TARGET): main.o memalloc.o
	$(CC) $(CFLAGS) -o $(TARGET) main.o memalloc.o

main.o: main.c memalloc.h
	$(CC) $(CFLAGS) -c main.c

memalloc.o: memalloc.c memalloc.h
	$(CC) $(CFLAGS) -c memalloc.c

clean:
	rm -f *.o $(TARGET)
