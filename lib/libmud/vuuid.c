/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * Copyright (c) 2002 Hiten Mahesh Pandya
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
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vassert.h"
#include "vuuid.h"

/*
 * uuid_from_string() - convert a string representation of an UUID into
 *			a binary representation.
 * See also:
 *	http://www.opengroup.org/onlinepubs/009629399/uuid_from_string.htm
 *
 * NOTE: The sequence field is in big-endian, while the time fields are in
 *	 native byte order.
 */
void
VUUID_from_string(const char *s, vuuid_t *u, uint32_t *status)
{
	int n;

	assert(s != NULL);
	assert(*s != '\0');

	/* Assume the worst. */
	if (status != NULL)
		*status = vuuid_s_invalid_string_uuid;

	/* The UUID string representation has a fixed length. */
	if (strlen(s) != 36)
		return;

	/*
	 * We only work with "new" UUIDs. New UUIDs have the form:
	 *	01234567-89ab-cdef-0123-456789abcdef
	 * The so called "old" UUIDs, which we don't support, have the form:
	 *	0123456789ab.cd.ef.01.23.45.67.89.ab
	 */
	if (s[8] != '-')
		return;

	n = sscanf(s,
	    "%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
	    &u->time_low, &u->time_mid, &u->time_hi_and_version,
	    &u->clock_seq_hi_and_reserved, &u->clock_seq_low, &u->node[0],
	    &u->node[1], &u->node[2], &u->node[3], &u->node[4], &u->node[5]);

	/* Make sure we have all conversions. */
	if (n != 11)
		return;

	/* We have a successful scan. Check semantics... */
	n = u->clock_seq_hi_and_reserved;
	if ((n & 0x80) != 0x00 &&			/* variant 0? */
	    (n & 0xc0) != 0x80 &&			/* variant 1? */
	    (n & 0xe0) != 0xc0) {			/* variant 2? */
		if (status != NULL)
			*status = vuuid_s_bad_version;
	} else {
		if (status != NULL)
			*status = vuuid_s_ok;
	}
}

/*
 * uuid_is_nil() - return whether the UUID is a nil UUID.
 * See also:
 *	http://www.opengroup.org/onlinepubs/009629399/uuid_is_nil.htm
 */
int32_t
VUUID_is_nil(const vuuid_t *u)
{
	const uint32_t *p;

	if (!u)
		return (1);

	/*
	 * Pick the largest type that has equivalent alignment constraints
	 * as an UUID and use it to test if the UUID consists of all zeroes.
	 */
	p = (const uint32_t*)u;
	return ((p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 0) ? 1 : 0);
}

/* A macro used to improve the readability of uuid_compare(). */
#define DIFF_RETURN(a, b, field)	do {			\
	if ((a)->field != (b)->field)				\
		return (((a)->field < (b)->field) ? -1 : 1);	\
} while (0)

/*
 * uuid_compare() - compare two UUIDs.
 * See also:
 *	http://www.opengroup.org/onlinepubs/009629399/uuid_compare.htm
 *
 * NOTE: Either UUID can be NULL, meaning a nil UUID. nil UUIDs are smaller
 *	 than any non-nil UUID.
 */
int32_t
VUUID_compare(const vuuid_t *a, const vuuid_t *b)
{
	int	res;

	/* Deal with NULL or equal pointers. */
	if (a == b)
		return (0);
	if (a == NULL)
		return ((VUUID_is_nil(b)) ? 0 : -1);
	if (b == NULL)
		return ((VUUID_is_nil(a)) ? 0 : 1);

	/* We have to compare the hard way. */
	DIFF_RETURN(a, b, time_low);
	DIFF_RETURN(a, b, time_mid);
	DIFF_RETURN(a, b, time_hi_and_version);
	DIFF_RETURN(a, b, clock_seq_hi_and_reserved);
	DIFF_RETURN(a, b, clock_seq_low);

	res = memcmp(a->node, b->node, sizeof(a->node));
	if (res)
		return ((res < 0) ? -1 : 1);
	return (0);
}

int
VUUID_genstr(char *buf, size_t bufmax)
{
	int i;
	char v[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a',
		     'b', 'c', 'd', 'e', 'f' };

	if (bufmax < VUUID_STR_LEN)
		return (-1);
	for (i = 0; i < VUUID_STR_LEN - 1; ++i)
		buf[i] = v[rand() % 16];
	buf[8] = '-';
	buf[13] = '-';
	buf[18] = '-';
	buf[23] = '-';
	buf[VUUID_STR_LEN - 1] = '\0';
	return (0);
}

void
VUUID_to_string(const vuuid_t *u, char *s, size_t slen)
{

	assert(s != NULL);
	assert(u != NULL);

	snprintf(s, slen, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    u->time_low, u->time_mid, u->time_hi_and_version,
	    u->clock_seq_hi_and_reserved, u->clock_seq_low, u->node[0],
	    u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
}
