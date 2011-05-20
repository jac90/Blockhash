rm=/bin/rm -f
CC= gcc
DEFS=  
PROGNAME= lib
INCLUDES=  -I../include
LIBS= -L. -lhashfs
LIBNAME=libhashfs.so
LIBVERSION=1.0.1


DEFINES= $(INCLUDES) $(DEFS)
CFLAGS= -g $(DEFINES)

SRCS := crc32.c 
SRCS += sha1.c 
SRCS += hashutil.c 
SRCS += fsutil.c 

BASE_OBJS := crc32.o 
BASE_OBJS += sha1.o 
BASE_OBJS += hashutil.o
BASE_OBJS += fsutil.o

.c.o:
	$(CC) $(CFLAGS) -c -fPIC $*.c

all: $(PROGNAME)

lib : $(BASE_OBJS)
	$(CC) -shared -Wl,-soname,$(LIBNAME) -o $(LIBNAME) $(BASE_OBJS)
 
clean:
	$(rm) $(PROGNAME) core *.o *.so *~
