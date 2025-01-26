/*
 * Copyright (c) 2021 Daniel Hope (www.floorsense.nz, daniel.hope@smartalock.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of "Floorsense Ltd", "Agile Workspace Ltd" nor
 *    the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "odr.h"
#include "vassert.h"

#include "crypto.h"
#include "wireguard.h"
#include "wireguard-pbuf.h"

struct wireguard_pbuf_stat {
	uint64_t	n_pbuf_cache_count;
};
static struct wireguard_pbuf_stat wg_pbuf_stat;

VTAILQ_HEAD(pbuf_cache_head, pbuf_cache);
static struct pbuf_cache_head pbuf_cache_head[PBUF_CACHE_HEAD_SIZE];

void
PBUF_init(void)
{
	int i;

	for (i = 0; i < PBUF_CACHE_HEAD_SIZE; i++) {
		VTAILQ_INIT(&pbuf_cache_head[i]);
	}
}

struct pbuf *
pbuf_alloc(size_t size)
{
	struct pbuf_cache_head *head;
	struct pbuf_cache *cache;
	struct pbuf *p;

	assert(size >= 0);
	assert(size <= PBUF_CACHE_HEAD_SIZE - 1);
	head = &pbuf_cache_head[size];
	if (!VTAILQ_EMPTY(head)) {
		cache = VTAILQ_FIRST(head);
		assert(cache->magic == PBUF_CACHE_MAGIC);
		assert(cache->size == size);
		VTAILQ_REMOVE(head, cache, list);
		wg_pbuf_stat.n_pbuf_cache_count--;
		p = (struct pbuf *)(cache + 1);
		assert(p->ptr == (uint8_t *)(p + 1));
		p->payload = p->ptr + 128;
		p->len = size;
		p->tot_len = size;
		AZ(p->next);
		return (p);
	}
	cache = (struct pbuf_cache *)malloc(sizeof(*cache) + sizeof(*p) +
	    size + 256);
	cache->magic = PBUF_CACHE_MAGIC;
	cache->size = (uint16_t)size;
	p = (struct pbuf *)(cache + 1);
	AN(p);
	p->ptr = (uint8_t *)(p + 1);
	AN(p->ptr);
	p->payload = p->ptr + 128;
	p->len = size;
	p->tot_len = size;
	p->next = NULL;
	return (p);
}

int
pbuf_take(struct pbuf *buf, const void *dataptr, uint16_t len)
{
	struct pbuf *p;
	size_t buf_copy_len;
	size_t total_copy_len = len;
	size_t copied_total = 0;

	if (buf == NULL || dataptr == NULL || buf->tot_len < len)
		return (-1);
	for (p = buf; total_copy_len != 0; p = p->next) {
		buf_copy_len = total_copy_len;
		if (buf_copy_len > p->len) {
			buf_copy_len = p->len;
		}
		memcpy(p->payload, &((const char *)dataptr)[copied_total],
		    buf_copy_len);
		total_copy_len -= buf_copy_len;
		copied_total += buf_copy_len;
	}
	assert(total_copy_len == 0 && copied_total == len);
	return (0);
}

uint16_t
pbuf_copy_partial(const struct pbuf *buf, void *dataptr, uint16_t len,
    uint16_t offset)
{
	const struct pbuf *p;
	uint16_t left = 0;
	uint16_t buf_copy_len;
	uint16_t copied_total = 0;

	assert(buf != NULL);
	assert(dataptr != NULL);

	for (p = buf; len != 0 && p != NULL; p = p->next) {
		if ((offset != 0) && (offset >= p->len)) {
			offset = (uint16_t)(offset - p->len);
		} else {
			buf_copy_len = (uint16_t)(p->len - offset);
			if (buf_copy_len > len) {
				buf_copy_len = len;
			}
			memcpy(&((char *)dataptr)[left],
			    &((char *)p->payload)[offset], buf_copy_len);
			copied_total = (uint16_t)(copied_total + buf_copy_len);
			left = (uint16_t)(left + buf_copy_len);
			len = (uint16_t)(len - buf_copy_len);
			offset = 0;
		}
	}
	return copied_total;
}

void
pbuf_free(struct pbuf *p)
{
	struct pbuf_cache *cache;

	cache = (struct pbuf_cache *)(((uint8_t *)p) - sizeof(*cache));
	assert(cache->magic == PBUF_CACHE_MAGIC);
	assert(cache->size >= 0);
	assert(cache->size <= PBUF_CACHE_HEAD_SIZE - 1);
	VTAILQ_INSERT_HEAD(&pbuf_cache_head[cache->size], cache, list);
	wg_pbuf_stat.n_pbuf_cache_count++;
}
