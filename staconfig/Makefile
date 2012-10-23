CC      = cc
OBJS    = staconfig.o
CFLAGS  = -O0 -g -Wall -W
LDFLAGS = -lm

.PHONY: all clean tags doc

all: staconfig

staconfig: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o

tags:
	etags *.c *.h

doc:
	doxygen staconfig.Doxyfile
