NCLUDES = -I

CFLAGS = -g -O0 -Wall -Wstrict-prototypes -Wmissing-prototypes		\
	 -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings	\
	 -Wswitch -Wshadow -Wcast-align -Wunused-parameter		\
	 -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls	\
	 -Wformat							\
	 -I. -I../../../lib/libjansson -I../../../lib/libmud
DEFINES = -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
OBJS = \
	../../../lib/libjansson/dump.o \
	../../../lib/libjansson/error.o \
	../../../lib/libjansson/hashtable.o \
	../../../lib/libjansson/hashtable_seed.o \
	../../../lib/libjansson/load.o \
	../../../lib/libjansson/memory.o \
	../../../lib/libjansson/pack_unpack.o \
	../../../lib/libjansson/strbuffer.o \
	../../../lib/libjansson/strconv.o \
	../../../lib/libjansson/utf.o \
	../../../lib/libjansson/value.o \
	../../../lib/libmud/linux/vpf.o \
	../../../lib/libmud/odr.o \
	../../../lib/libmud/odr_pthread.o \
	../../../lib/libmud/vsb.o \
	../../../lib/libmud/vtc_log.o \
	../../../lib/libmud/vtim.o \
	mudband_service_cmdctl.o \
	mudband_service.o

all: mudband_service

mudband_service: $(OBJS)
	gcc $(CFLAGS) $(DEFINES) -o mudband_service $(OBJS) -lm -lpthread

.c.o:
	gcc $(CFLAGS) $(DEFINES) -c -o $@ $<

clean:
	rm -f mudband_service $(OBJS) *~

