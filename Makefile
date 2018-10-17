CFLAGS=-g -O2
LDFLAGS=-pthread $(CFLAGS)
line: line.o

%.o: %.c %.h
