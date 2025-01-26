/*-
 * Copyright (c) 2010-2014 Weongyo Jeong <weongyo@gmail.com>
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

#ifndef _LIBMUD_ASSERT_H
#define	_LIBMUD_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

void	VAS_Fail(const char *func, const char *file, int line, const char *cond,
	    int xxx);

#ifdef __cplusplus
};
#endif

#ifdef WITHOUT_ASSERTS
#define	assert(e)	((void)(e))
#else /* WITH_ASSERTS */
#if defined(WIN32)
#define	__func__	__FUNCTION__
#endif
#undef assert
#define	assert(e)							\
do {									\
	if (!(e))							\
		VAS_Fail(__func__, __FILE__, __LINE__, #e, 0);		\
} while (0)
#endif

#endif

