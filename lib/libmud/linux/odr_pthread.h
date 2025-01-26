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

#ifndef _OS_LINUX_PTHREAD_H_
#define	_OS_LINUX_PTHREAD_H_

struct odr_pthread_key { void *priv; };
typedef struct odr_pthread_key odr_pthread_key_t;

struct odr_pthread_cond { void *priv; };
typedef struct odr_pthread_cond *odr_pthread_cond_t;

struct odr_pthread_mutex { void *priv; };
typedef struct odr_pthread_mutex *odr_pthread_mutex_t;

struct odr_pthread_mutex_attr { void *priv; };
typedef struct odr_pthread_mutex_attr *odr_pthread_mutexattr_t;
#define	ODR_PTHREAD_MUTEX_RECURSIVE	(1 << 0)

struct odr_pthread { void *priv; };
typedef struct odr_pthread *odr_pthread_t;

int	ODR_pthread_setspecific(odr_pthread_key_t key, const void *value);
int	ODR_pthread_key_create(odr_pthread_key_t *key,
	    void (*destructor)(void *));
void *	ODR_pthread_getspecific(odr_pthread_key_t key);
void *	ODR_pthread_self(void);
int	ODR_pthread_equal(void *t1, void *t2);

int	ODR_pthread_cond_init(odr_pthread_cond_t *cond, const void *attr);
int	ODR_pthread_cond_signal(odr_pthread_cond_t *cond);
int	ODR_pthread_cond_wait(odr_pthread_cond_t *cond,
	    odr_pthread_mutex_t *mutex);
int	ODR_pthread_cond_timedwait(odr_pthread_cond_t *cond,
	    odr_pthread_mutex_t *mutex, const void *abstime);
int	ODR_pthread_cond_broadcast(odr_pthread_cond_t *cond);
int	ODR_pthread_cond_destroy(odr_pthread_cond_t *cond);

int	ODR_pthread_mutex_init(odr_pthread_mutex_t *mutex,
	    const odr_pthread_mutexattr_t *attr);
int	ODR_pthread_mutex_lock(odr_pthread_mutex_t *mutex);
int	ODR_pthread_mutex_unlock(odr_pthread_mutex_t *mutex);
int	ODR_pthread_mutex_trylock(odr_pthread_mutex_t *mutex);
int	ODR_pthread_mutex_destroy(odr_pthread_mutex_t *mutex);
int	ODR_pthread_mutexattr_init(odr_pthread_mutexattr_t *attr);
int	ODR_pthread_mutexattr_settype(odr_pthread_mutexattr_t *attr, int type);
int	ODR_pthread_mutexattr_destroy(odr_pthread_mutexattr_t *attr);

int	ODR_pthread_create(odr_pthread_t *thread, const void *attr,
	    void *(*start_routine)(void *), void *arg);
int	ODR_pthread_join(odr_pthread_t thread, void **value_ptr);
int	ODR_pthread_detach(odr_pthread_t thread);
int	ODR_pthread_detach_self(void);
void	ODR_pthread_free(odr_pthread_t thread);

#endif
