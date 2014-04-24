CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lm -lrt -pthread
SOURCES=prod_cons.c libcv/dl_syscalls.c rt-app_utils.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=prod_cons

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) 

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)

distclean:
	rm -rf *.o *.dat $(EXECUTABLE)
