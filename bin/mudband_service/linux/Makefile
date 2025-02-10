NCLUDES = -I

CFLAGS = -g -O0 -Wall -Wstrict-prototypes -Wmissing-prototypes		\
	 -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings	\
	 -Wswitch -Wshadow -Wcast-align -Wunused-parameter		\
	 -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls	\
	 -Wformat							\
	 -I. -I../../../lib/libjansson -I../../../lib/libmud \
	 -I../../mudband/common
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
	../../../lib/libmud/vct.o \
	../../../lib/libmud/vhttps.o \
	../../../lib/libmud/vopt.o \
	../../../lib/libmud/vsb.o \
	../../../lib/libmud/vsock.o \
	../../../lib/libmud/vss.o \
	../../../lib/libmud/vssl.o \
	../../../lib/libmud/vtc_log.o \
	../../../lib/libmud/vtim.o \
	../../mudband/common/crypto/blake2s.o \
	../../mudband/common/crypto/chacha20.o \
	../../mudband/common/crypto/chacha20poly1305.o \
	../../mudband/common/crypto/poly1305-donna.o \
	../../mudband/common/crypto/x25519.o \
	../../mudband/common/crypto.o \
	../../mudband/common/wireguard.o \
	mudband_service_confmgr.o \
	mudband_service_cmdctl.o \
	mudband_service_enroll.o \
	mudband_service_progconf.o \
	mudband_service.o

all: mudband_service

mudband_service: $(OBJS)
	gcc $(CFLAGS) $(DEFINES) -o mudband_service $(OBJS) \
	    -lssl -lcrypto -lpthread -lm -lrt

.c.o:
	gcc $(CFLAGS) $(DEFINES) -c -o $@ $<

pkg_debian:
	debuild -uc -us -b

clean:
	rm -f mudband_service $(OBJS) *~

