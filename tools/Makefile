rm=/bin/rm -f
CC= gcc
DEFS=  
PROGNAME= create extract create_hashfs extract_hashfs create_single_hashnode
INCLUDES=  -I../include
LIBS= -L../lib -lhashfs
LIBNAME=libhashfs.so
LIBVERSION=1.0.1


DEFINES= $(INCLUDES) $(DEFS)
CFLAGS= -g $(DEFINES)

SRCS += create.c 
SRCS += extract.c 
SRCS += create_hashfs.c 
SRCS += create_single_hashnode.c
SRCS += extract_hashfs.c 

CREAT_OBJS = create.o
EXTRACT_OBJS = extract.o
CREATHASH_OBJS = create_hashfs.o create_single_hashnode.o
EXTRACTHASH_OBJS = extract_hashfs.o

.c.o:
	$(CC) $(CFLAGS) -c -fPIC $*.c

all: $(PROGNAME)

create : $(CREAT_OBJS)
	$(CC) $(CFLAGS) -o create $(CREAT_OBJS) $(LIBS)

extract : $(EXTRACT_OBJS)
	$(CC) $(CFLAGS) -o extract $(EXTRACT_OBJS) $(LIBS)

create_hashfs : $(CREATHASH_OBJS)
	$(CC) $(CFLAGS) -o create_hashfs create_hashfs.o $(LIBS)
	$(CC) $(CFLAGS) -o create_single_hashnode create_single_hashnode.o $(LIBS)

extract_hashfs : $(EXTRACTHASH_OBJS)
	$(CC) $(CFLAGS) -o extract_hashfs $(EXTRACTHASH_OBJS) $(LIBS)

clean:
	$(rm) $(PROGNAME) core *.o *.so *~
