/*-
 * Copyright (c) 2011-2014 Weongyo Jeong <weongyo@gmail.com>
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
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"

struct thread_cond {
	unsigned		magic;
#define	COND_MAGIC		0x5dc4ff7a
	pthread_cond_t		cond;
};

struct thread_thr {
	unsigned		magic;
#define	THREAD_MAGIC		0xf29b2aed
	pthread_t		thread;
};

struct thread_mutex {
	unsigned		magic;
#define	MUTEX_MAGIC		0x4ee8f1c0
	pthread_mutex_t		mtx;
};

struct thread_mutex_attr {
	unsigned		magic;
#define	MUTEXATTR_MAGIC		0x11bbb73c
	pthread_mutexattr_t	attr;
};

int
ODR_pthread_create(odr_pthread_t *thread, const void *attr,
    void *(*start_routine)(void *), void *arg)
{
	struct thread_thr *it;
	odr_pthread_t t;

	assert(attr == NULL);

	t = malloc(sizeof(*t));
	assert(t != NULL);
	it = malloc(sizeof(*it));
	assert(it != NULL);
	it->magic = THREAD_MAGIC;
	t->priv = it;
	*thread = t;
	assert(pthread_create(&it->thread, attr, start_routine, arg) == 0);
	return (0);
}

int
ODR_pthread_detach(odr_pthread_t thread)
{
	struct thread_thr *it;

	it = thread->priv;
	assert(it->magic == THREAD_MAGIC);
	return (pthread_detach(it->thread));
}

void
ODR_pthread_free(odr_pthread_t thread)
{
	struct thread_thr *it;

	it = thread->priv;
	assert(it->magic == THREAD_MAGIC);
	free(it);
	free(thread);
}

/*****************************************************************************/

int
ODR_pthread_mutex_init(odr_pthread_mutex_t *mutex,
    const odr_pthread_mutexattr_t *attr)
{
	struct thread_mutex *im;
	struct thread_mutex_attr *ia;
	odr_pthread_mutex_t m;
	odr_pthread_mutexattr_t a;;
	
	m = malloc(sizeof(*m));
	assert(m != NULL);
	im = malloc(sizeof(*im));
	assert(im != NULL);
	im->magic = MUTEX_MAGIC;

	if (attr != NULL) {
		a = *attr;
		assert(a != NULL);
		ia = a->priv;
		assert(ia->magic == MUTEXATTR_MAGIC);
		assert(pthread_mutex_init(&im->mtx, &ia->attr) == 0);
	} else
		assert(pthread_mutex_init(&im->mtx, NULL) == 0);
	m->priv = im;
	*mutex = m;
	return (0);
}

int
ODR_pthread_mutex_lock(odr_pthread_mutex_t *mutex)
{
	struct thread_mutex *im;

	im = (*mutex)->priv;
	assert(im->magic == MUTEX_MAGIC);
	return (pthread_mutex_lock(&im->mtx));
}

int
ODR_pthread_mutex_unlock(odr_pthread_mutex_t *mutex)
{
	struct thread_mutex *im;

	im = (*mutex)->priv;
	assert(im->magic == MUTEX_MAGIC);
	return (pthread_mutex_unlock(&im->mtx));
}

int
ODR_pthread_mutex_trylock(odr_pthread_mutex_t *mutex)
{
	struct thread_mutex *im;

	im = (*mutex)->priv;
	assert(im->magic == MUTEX_MAGIC);
	return (pthread_mutex_trylock(&im->mtx));
}

int
ODR_pthread_mutex_destroy(odr_pthread_mutex_t *mutex)
{
	struct thread_mutex *im;

	im = (*mutex)->priv;
	assert(im->magic == MUTEX_MAGIC);
	assert(pthread_mutex_destroy(&im->mtx) == 0);
	free(im);
	free(*mutex);
	*mutex = NULL;

	return (0);
}

int
ODR_pthread_mutexattr_init(odr_pthread_mutexattr_t *attr)
{
	struct thread_mutex_attr *ia;
	odr_pthread_mutexattr_t a;

	a = malloc(sizeof(*a));
	assert(a != NULL);
	ia = malloc(sizeof(*ia));
	assert(ia != NULL);
	ia->magic = MUTEXATTR_MAGIC;
	assert(pthread_mutexattr_init(&ia->attr) == 0);
	a->priv = ia;
	*attr = a;
	return (0);
}

int
ODR_pthread_mutexattr_settype(odr_pthread_mutexattr_t *attr, int type)
{
	struct thread_mutex_attr *ia;
	int value = -1;

	ia = (*attr)->priv;
	assert(ia->magic == MUTEXATTR_MAGIC);

	if ((type & ODR_PTHREAD_MUTEX_RECURSIVE) != 0)
		value = PTHREAD_MUTEX_RECURSIVE;

	assert(value != -1);

	return (pthread_mutexattr_settype(&ia->attr, value));
}

int
ODR_pthread_mutexattr_destroy(odr_pthread_mutexattr_t *attr)
{
	struct thread_mutex_attr *ia;

	ia = (*attr)->priv;
	assert(ia->magic == MUTEXATTR_MAGIC);
	assert(pthread_mutexattr_destroy(&ia->attr) == 0);
	free(ia);
	free(*attr);
	*attr = NULL;

	return (0);
}

void *
ODR_pthread_self(void)
{

	return ((void *)pthread_self());
}

int
ODR_pthread_equal(void *t1, void *t2)
{

	return (pthread_equal((pthread_t)t1, (pthread_t)t2));
}

int
ODR_pthread_cond_wait(odr_pthread_cond_t *cond, odr_pthread_mutex_t *mutex)
{
	struct thread_cond *ic;
	struct thread_mutex *im;

	ic = (*cond)->priv;
	assert(ic->magic == COND_MAGIC);
	im = (*mutex)->priv;
	assert(im->magic == MUTEX_MAGIC);

	return (pthread_cond_wait(&ic->cond, &im->mtx));
}

int
ODR_pthread_cond_timedwait(odr_pthread_cond_t *cond, odr_pthread_mutex_t *mutex,
    const void *abstime_arg)
{
	const struct odr_timespec *abstime =
	    (const struct odr_timespec *)abstime_arg;
	struct thread_cond *ic;
	struct thread_mutex *im;
	struct timespec tv;
	int ret;

	tv.tv_sec = abstime->tv_sec;
	tv.tv_nsec = abstime->tv_nsec;

	ic = (*cond)->priv;
	assert(ic->magic == COND_MAGIC);
	im = (*mutex)->priv;
	assert(im->magic == MUTEX_MAGIC);

	ret = pthread_cond_timedwait(&ic->cond, &im->mtx, &tv);
	if (ret == ETIMEDOUT)
		return (ODR_ETIMEDOUT);
	if (ret == 0)
		return (0);
	assert(0 == 1);
	return (-1);
}

int
ODR_pthread_join(odr_pthread_t thread, void **value_ptr)
{
	struct thread_thr *it;

	it = thread->priv;
	assert(it->magic == THREAD_MAGIC);
	return (pthread_join(it->thread, value_ptr));
}

int
ODR_pthread_cond_signal(odr_pthread_cond_t *cond)
{
	struct thread_cond *ic;

	ic = (*cond)->priv;
	assert(ic->magic == COND_MAGIC);
	return (pthread_cond_signal(&ic->cond));
}

int
ODR_pthread_cond_init(odr_pthread_cond_t *cond, const void *attr)
{
	struct thread_cond *ic;
	odr_pthread_cond_t c;

	assert(attr == NULL);

	c = malloc(sizeof(*c));
	assert(c != NULL);
	ic = malloc(sizeof(*ic));
	assert(ic != NULL);
	ic->magic = COND_MAGIC;
	assert(pthread_cond_init(&ic->cond, NULL) == 0);
	c->priv = ic;
	*cond = c;
	return (0);
}

int
ODR_pthread_detach_self(void)
{

	return (pthread_detach(pthread_self()));
}

