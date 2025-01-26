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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mpo.h"
#include "odr.h"

#define	AZ(foo)		do { assert((foo) == 0); } while (0)
#define	AN(foo)		do { assert((foo) != 0); } while (0)

struct mpo *
MPO_init(void)
{
	struct mpo *mpo;

	mpo = malloc(sizeof(*mpo));
	AN(mpo);
	mpo->magic = MPO_MAGIC;
	VTAILQ_INIT(&mpo->bufs);

	return (mpo);
}

void *
MPO_malloc(struct mpo *mpo, size_t size)
{
	struct mpo_buf *mpb;

	mpb = malloc(sizeof(*mpb) + size);
	if (mpb == NULL)
		return (NULL);
	mpb->magic = MPO_BUF_MAGIC;
	VTAILQ_INSERT_TAIL(&mpo->bufs, mpb, list);
	return ((void *)(mpb + 1));
}

void *
MPO_calloc(struct mpo *mpo, size_t nmemb, size_t size)
{
	struct mpo_buf *mpb;

	mpb = malloc(sizeof(*mpb) + (nmemb * size));
	if (mpb == NULL)
		return (NULL);
	mpb->magic = MPO_BUF_MAGIC;
	memset(mpb + 1, 0, nmemb * size);
	VTAILQ_INSERT_TAIL(&mpo->bufs, mpb, list);
	return ((void *)(mpb + 1));
}

void *
MPO_realloc(struct mpo *mpo, void *ptr, size_t size)
{
	struct mpo_buf *mpb;

	if (ptr == NULL)
		return (MPO_malloc(mpo, size));
	if (size == 0) {
		MPO_free(mpo, ptr);
		return (NULL);
	}
	mpb = (struct mpo_buf *)(((char *)ptr) - sizeof(*mpb));
	assert(mpb->magic == MPO_BUF_MAGIC);
	mpb = realloc(mpb, sizeof(*mpb) + size);
	return ((void *)(mpb + 1));
}

char *
MPO_strdup(struct mpo *mpo, const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if ((copy = MPO_malloc(mpo, len)) == NULL)
		return (NULL);
	memcpy(copy, str, len);
	return (copy);
}

char *
MPO_strndup(struct mpo *mpo, const char *str, size_t n)
{
	size_t len;
	char *copy;

	len = strnlen(str, n);
	if ((copy = MPO_malloc(mpo, len + 1)) == NULL)
		return (NULL);
	memcpy(copy, str, len);
	copy[len] = '\0';
	return (copy);
}

void
MPO_free(struct mpo *mpo, void *ptr)
{
	struct mpo_buf *mpb;

	if (ptr == NULL)
		return;
	mpb = (struct mpo_buf *)(((char *)ptr) - sizeof(*mpb));
	assert(mpb->magic == MPO_BUF_MAGIC);
	VTAILQ_REMOVE(&mpo->bufs, mpb, list);
	free(mpb);
}

void
MPO_fini(struct mpo *mpo)
{
	struct mpo_buf *mpb;

	assert(mpo->magic == MPO_MAGIC);

	while (!VTAILQ_EMPTY(&mpo->bufs)) {
		mpb = VTAILQ_FIRST(&mpo->bufs);
		VTAILQ_REMOVE(&mpo->bufs, mpb, list);
		free(mpb);
	}
	free(mpo);
}
