CC      = cc
OBJS    = stamanagement.o sta_timer.o
CFLAGS  = -O0 -g -Wall -W -ftrapv
LDFLAGS = -lpthread -lm

.PHONY: all clean tags doc

all: stamd

stamd: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o

tags:
	etags *.c *.h

doc:
	doxygen stamanagement-doc.Doxyfile
