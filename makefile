# the compiler to use
CC = gcc

# compiler flags:
#  -ggdb3 adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -ggdb3 -Wall
  
# linker flags:
LFLAGS =

# libraries to link with
LIBS =
  
# the name to use for both the target source file, and the output file:
TARGET = travelMonitor

# source files
SRC = src/travelMonitor.c lib/lists/lists.c

# object files
OBJ = travelMonitor.o lists.o

# the name to use for both the target source file, and the output file:
TARGET2 = monitor

# source files
SRC2 = src/monitor.c lib/lists/lists.c lib/hashtable/hashtable.c lib/bloomfilter/bloomfilter.c \
	src/parser.c src/vaccineMonitor.c lib/skiplist/skiplist.c lib/date/date.c lib/hash/hash.c

# object files
OBJ2 = monitor.o lists.o hashtable.o bloomfilter.o parser.o vaccineMonitor.o skiplist.o date.o hash.o

# make all by default
default: all

all: $(TARGET) $(TARGET2)

# make target
$(TARGET): $(OBJ)
	$(CC) $(LFLAGS) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# make object files
travelMonitor.o: src/travelMonitor.c
	$(CC) $(CFLAGS) -c src/travelMonitor.c
lists.o: lib/lists/lists.c
	$(CC) $(CFLAGS) -c lib/lists/lists.c

# make target
$(TARGET2): $(OBJ2)
	$(CC) $(LFLAGS) $(CFLAGS) -o $(TARGET2) $(OBJ2) $(LIBS)

# make object files
monitor.o: src/monitor.c
	$(CC) $(CFLAGS) -c src/monitor.c

hashtable.o: lib/lists/lists.c lib/hashtable/hashtable.c 
	$(CC) $(CFLAGS) -c lib/hashtable/hashtable.c

bloomfilter.o: lib/bloomfilter/bloomfilter.c lib/hash/hash.c
	$(CC) $(CFLAGS) -c lib/bloomfilter/bloomfilter.c

parser.o: src/parser.c lib/skiplist/skiplist.c lib/bloomfilter/bloomfilter.c
	$(CC) $(CFLAGS) -c src/parser.c

vaccineMonitor.o: src/vaccineMonitor.c lib/date/date.c
	$(CC) $(CFLAGS) -c src/vaccineMonitor.c

skiplist.o: lib/skiplist/skiplist.c
	$(CC) $(CFLAGS) -c lib/skiplist/skiplist.c

date.o: lib/date/date.c
	$(CC) $(CFLAGS) -c lib/date/date.c

hash.o: lib/hash/hash.c
	$(CC) $(CFLAGS) -c lib/hash/hash.c

# clean up
clean:
	-rm -f $(TARGET)
	-rm -f $(OBJ)
	-rm -f $(TARGET2)
	-rm -f $(OBJ2)
	-rm ./tmp/*
