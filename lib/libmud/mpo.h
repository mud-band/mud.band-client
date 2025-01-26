/*-
 * Copyright (c) 2015 Weongyo Jeong <weongyo@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: sys/vm/uma.h 213911 2010-10-16 04:41:45Z lstewart $
 *
 */

#ifndef _MPO_H
#define	_MPO_H

#include "vqueue.h"

struct mpo_buf {
	uint32_t		magic;
#define	MPO_BUF_MAGIC		0x67a3798b
	VTAILQ_ENTRY(mpo_buf)	list;
};

struct mpo {
	uint32_t		magic;
#define	MPO_MAGIC		0xfc522413
	VTAILQ_HEAD(, mpo_buf)	bufs;
};

struct mpo *
	MPO_init(void);
void	MPO_fini(struct mpo *mpo);
void *	MPO_malloc(struct mpo *mpo, size_t size);
void *	MPO_calloc(struct mpo *mpo, size_t nmemb, size_t size);
void *	MPO_realloc(struct mpo *mpo, void *ptr, size_t size);
char *	MPO_strdup(struct mpo *mpo, const char *s);
char *	MPO_strndup(struct mpo *mpo, const char *s, size_t n);
void	MPO_free(struct mpo *mpo, void *ptr);

#endif /* _MPO_H */
