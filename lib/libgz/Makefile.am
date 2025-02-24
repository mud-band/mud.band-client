#

noinst_LTLIBRARIES = libgz.la

libgz_la_LDFLAGS = -avoid-version
libgz_la_CFLAGS = -D_LARGEFILE64_SOURCE=1 $(libvgz_extra_cflags)

libgz_la_SOURCES = \
	adler32.c \
	compress.c \
	crc32.c \
	crc32.h \
	deflate.c \
	deflate.h \
	infback.c \
	inffast.c \
	inffast.h \
	inffixed.h \
	inflate.c \
	inflate.h \
	inftrees.c \
	inftrees.h \
	trees.c \
	trees.h \
	uncompr.c \
	zconf.h \
	vgz.h \
	zutil.c \
	zutil.h
