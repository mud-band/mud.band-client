/*
 * Copyright (c) 2021 Daniel Hope (www.floorsense.nz, daniel.hope@smartalock.com)
 * Copyright (c) 2024 Weongyo Jeong (weongyo@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#ifndef _WIREGUARD_PBUF_H_
#define _WIREGUARD_PBUF_H_

#define	PBUF_CACHE_MAGIC			0xd8e6a235
#define	PBUF_CACHE_HEAD_SIZE			(2048 + 1)
struct pbuf_cache {
	unsigned	magic;
	uint16_t	size;
	VTAILQ_ENTRY(pbuf_cache) list;
};

struct pbuf {
	uint8_t		*ptr;
	uint8_t		*payload;
	size_t		len;
	size_t		tot_len;
	struct pbuf	*next;
};

void	PBUF_init(void);
struct pbuf *
	pbuf_alloc(size_t size);
int	pbuf_take(struct pbuf *buf, const void *dataptr, uint16_t len);
uint16_t
	pbuf_copy_partial(const struct pbuf *buf, void *dataptr, uint16_t len,
	    uint16_t offset);
void	pbuf_free(struct pbuf *p);

#endif
