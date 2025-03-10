NCLUDES = -I

CFLAGS = -g -O3 -Wall -Wstrict-prototypes -Wmissing-prototypes		\
	 -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings	\
	 -Wswitch -Wshadow -Wcast-align -Wunused-parameter		\
	 -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls	\
	 -Wformat \
	 -I. -I../../../lib/libjansson -I../../../lib/libmud \
	 -I../../../lib/libgz -I../common
DEFINES = -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
OBJS =  \
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
	../../../lib/libgz/adler32.o \
	../../../lib/libgz/compress.o \
	../../../lib/libgz/crc32.o \
	../../../lib/libgz/deflate.o \
	../../../lib/libgz/infback.o \
	../../../lib/libgz/inffast.o \
	../../../lib/libgz/inflate.o \
	../../../lib/libgz/inftrees.o \
	../../../lib/libgz/trees.o \
	../../../lib/libgz/uncompr.o \
	../../../lib/libgz/zutil.o \
	../../../lib/libmud/odr.o \
	../../../lib/libmud/odr_pthread.o \
	../../../lib/libmud/linux/vpf.o \
	../../../lib/libmud/vct.o \
	../../../lib/libmud/vhttps.o \
	../../../lib/libmud/vopt.o \
	../../../lib/libmud/vsb.o \
	../../../lib/libmud/vsock.o \
	../../../lib/libmud/vss.o \
	../../../lib/libmud/vssl.o \
	../../../lib/libmud/vtc_log.o \
	../../../lib/libmud/vtim.o \
	../../../lib/libmud/vuuid.o \
	../common/callout.o \
	../common/crypto/blake2s.o \
	../common/crypto/chacha20.o \
	../common/crypto/chacha20poly1305.o \
	../common/crypto/poly1305-donna.o \
	../common/crypto/x25519.o \
	../common/crypto.o \
	../common/mudband_bpf.o \
	../common/wireguard.o \
	../common/wireguard-pbuf.o \
	mudband.o \
	mudband_acl.o \
	mudband_confmgr.o \
	mudband_connmgr.o \
	mudband_enroll.o \
	mudband_mqtt.o \
	mudband_progconf.o \
	mudband_stun_client.o \
	mudband_tasks.o \
	mudband_webcli.o

all: mudband

mudband: $(OBJS)
	gcc $(CFLAGS) $(DEFINES) -I. -o mudband $(OBJS) \
	    -lssl -lcrypto -lpthread -lm -lrt

.c.o:
	gcc $(CFLAGS) $(DEFINES) -c -o $@ $<

../../../lib/libmud/odr.o: ../../../lib/libmud/linux/odr.c

clean:
	rm -f mudband $(OBJS) *~

tags:
	etags *.[ch] ../../../lib/libjansson/*.[ch] ../../../lib/libmud/*.[ch] \
	    ../../../lib/libmud/linux/*.[ch] ../common/*.[ch]
