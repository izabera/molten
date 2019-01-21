CFLAGS=-g -O2 -Wall -fsanitize=address
CXXFLAGS=$(CFLAGS)
LDFLAGS=-pthread $(CFLAGS)
line: line.o

%.o: %.c %.h
