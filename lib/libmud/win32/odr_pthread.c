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

#include <assert.h>
#include <errno.h>
#include <process.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock.h>

#include "odr.h"
#include "odr_pthread.h"

#pragma warning(disable: 4311)
#pragma warning(disable: 4267)

#define ODR_pthread_cleanup_push(_rout, _arg)	do {			\
	opt_cleanup_t     _cleanup;					\
	opt_push_cleanup(&_cleanup,					\
	    (opt_cleanup_callback_t) (_rout), (_arg) );		\
} while (0)

#define ODR_pthread_cleanup_pop(_execute)	do {			\
	(void)opt_pop_cleanup(_execute);				\
} while (0)

#define PTHREAD_STACK_MIN			0
#define PTHREAD_DESTRUCTOR_ITERATIONS		4
#define PTHREAD_CANCELED			((void *) -1)
#define	PTHREAD_MUTEX_INITIALIZER		((odr_pthread_mutex_t) -1)
#define	PTHREAD_RECURSIVE_MUTEX_INITIALIZER	((odr_pthread_mutex_t) -2)
#define	PTHREAD_ERRORCHECK_MUTEX_INITIALIZER	((odr_pthread_mutex_t) -3)
#define	PTHREAD_COND_INITIALIZER		((odr_pthread_cond_t) -1)

#define OPT_EPS_EXIT	(1)
#define OPT_EPS_CANCEL	(2)

#define OPT_THREAD_REUSE_EMPTY			((opt_thread_t *) 1)

enum {
	PTHREAD_CREATE_JOINABLE       = 0,
	PTHREAD_CREATE_DETACHED       = 1,
	PTHREAD_INHERIT_SCHED         = 0,
	PTHREAD_EXPLICIT_SCHED        = 1,
	PTHREAD_SCOPE_PROCESS         = 0,
	PTHREAD_SCOPE_SYSTEM          = 1,
	PTHREAD_CANCEL_ENABLE         = 0,
	PTHREAD_CANCEL_DISABLE        = 1,
	PTHREAD_CANCEL_ASYNCHRONOUS   = 0,
	PTHREAD_CANCEL_DEFERRED       = 1,
	PTHREAD_PROCESS_PRIVATE       = 0,
	PTHREAD_PROCESS_SHARED        = 1,
	PTHREAD_BARRIER_SERIAL_THREAD = -1
};

enum {
	PTHREAD_MUTEX_FAST_NP,
	PTHREAD_MUTEX_RECURSIVE_NP,
	PTHREAD_MUTEX_ERRORCHECK_NP,
	PTHREAD_MUTEX_TIMED_NP = PTHREAD_MUTEX_FAST_NP,
	PTHREAD_MUTEX_ADAPTIVE_NP = PTHREAD_MUTEX_FAST_NP,
	PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_FAST_NP,
	PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
	PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
	PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
};

typedef enum pthread_state
{
	PTHREAD_STATE_INITIAL = 0,
	PTHREAD_STATE_RUNNING,
	PTHREAD_STATE_SUSPENDED,
	PTHREAD_STATE_CANCEL_PENDING,
	PTHREAD_STATE_CANCELING,
	PTHREAD_STATE_LAST
};

struct odr_sem {
	int			value;
	odr_pthread_mutex_t	lock;
	HANDLE			sem;
};
typedef struct odr_sem *sem_t;

struct odr_thread_params {
	odr_pthread_t		tid;
	void			*(*start) (void *);
	void			*arg;
};
typedef struct odr_thread_params odr_thread_params;

struct odr_pthread_key {
	unsigned		magic;
#define	KEY_MAGIC		0xd0803389
	DWORD			key;
	void			(*destructor)(void *);
	odr_pthread_mutex_t	keylock;
	void			*threads;
};

struct opt_thread {
	DWORD			thread;
	HANDLE			threadH;
	odr_pthread_t		pt_handle;
	struct opt_thread	*prev_reuse;
	volatile enum pthread_state state;
	void			*exit_status;
	odr_pthread_mutex_t	thread_lock;
	int			detach_state;
	int			sched_priority;
	int			cancel_state;
	odr_pthread_mutex_t	cancel_lock;
	HANDLE			cancel_event;
	int			implicit;
	void			*keys;
	jmp_buf			start_mark;
	void			*next_assoc;
};
typedef struct opt_thread opt_thread_t;

typedef void (*opt_cleanup_callback_t)(void *);
struct opt_cleanup {
	opt_cleanup_callback_t	routine;
	void			*arg;
	struct opt_cleanup	*prev;
};
typedef struct opt_cleanup opt_cleanup_t;

typedef struct {
	odr_pthread_mutex_t	*mutex_ptr;
	odr_pthread_cond_t	cv;
	int			*result_ptr;
} opt_cond_wait_cleanup_args_t;

struct sched_param {
	int			sched_priority;
};

struct odr_pthread_attr {
	unsigned long		valid;
	void			*stackaddr;
	size_t			stacksize;
	int			detachstate;
	struct sched_param	param;
	int			inheritsched;
	int			contentionscope;
};

struct odr_pthread_cond {
	unsigned		magic;
#define	COND_MAGIC		0x5dc4ff7a
	long			n_waiters_blocked;
	long			n_waiters_gone;
	long			n_waiters_to_unblock;
	sem_t			sem_block_queue;
	sem_t			sem_block_lock;
	odr_pthread_mutex_t	mtx_unblock_lock;
	odr_pthread_cond_t	next;
	odr_pthread_cond_t	prev;
};

struct odr_pthread_condattr {
	int			pshared;
};

struct odr_pthread_mutex {
	unsigned		magic;
#define	MUTEX_MAGIC		0x4ee8f1c0
	LONG			lock_idx;
	int			kind;
	int			recursive_count;
	odr_pthread_t		ownerThread;
	HANDLE			event;
};

struct odr_pthread_mutex_attr {
	unsigned		magic;
#define	MUTEXATTR_MAGIC		0x11bbb73c
	int			pshared;
	int			kind;
};

typedef struct {
	sem_t			sem;
	int			*result_ptr;
} sem_timedwait_cleanup_args_t;

struct thread_key_assoc {
	opt_thread_t		*thread;
	odr_pthread_key_t	key;
	struct thread_key_assoc	*next_key;
	struct thread_key_assoc	*next_thread;
};
typedef struct thread_key_assoc thread_key_assoc;

static CRITICAL_SECTION		opt_cond_list_lock;
static CRITICAL_SECTION		opt_thread_reuse_lock;
static CRITICAL_SECTION		opt_mutex_test_init_lock;
static CRITICAL_SECTION		opt_cond_test_init_lock;

static odr_pthread_key_t	opt_self_thread_key;
static odr_pthread_key_t	opt_cleanup_key;
static odr_pthread_cond_t	opt_cond_list_head;
static odr_pthread_cond_t	opt_cond_list_tail;

static struct odr_pthread_mutex_attr opt_recursive_mutexattr_s =
	{ MUTEXATTR_MAGIC, PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_RECURSIVE };
static struct odr_pthread_mutex_attr opt_errorcheck_mutexattr_s =
	{ MUTEXATTR_MAGIC, PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_ERRORCHECK };
static odr_pthread_mutexattr_t opt_recursive_mutexattr =
	&opt_recursive_mutexattr_s;
static odr_pthread_mutexattr_t opt_errorcheck_mutexattr =
	&opt_errorcheck_mutexattr_s;

static int		opt_initialized;

static opt_thread_t	*opt_thread_reuse_top = OPT_THREAD_REUSE_EMPTY;
static opt_thread_t	*opt_thread_reuse_bottom = OPT_THREAD_REUSE_EMPTY;

static int	sem_post(sem_t *sem);

static odr_pthread_t
opt_thread_reuse_pop(void)
{
	odr_pthread_t t = { NULL, 0 };

	EnterCriticalSection(&opt_thread_reuse_lock);
	if (OPT_THREAD_REUSE_EMPTY != opt_thread_reuse_top) {
		opt_thread_t * tp;

		tp = opt_thread_reuse_top;
		opt_thread_reuse_top = tp->prev_reuse;
		if (OPT_THREAD_REUSE_EMPTY == opt_thread_reuse_top)
			opt_thread_reuse_bottom = OPT_THREAD_REUSE_EMPTY;
		tp->prev_reuse = NULL;
		t = tp->pt_handle;
	}
	LeaveCriticalSection(&opt_thread_reuse_lock);
	return t;

}

static void
opt_thread_reuse_push(odr_pthread_t thread)
{
	opt_thread_t * tp = (opt_thread_t *) thread.p;
	odr_pthread_t t;

	EnterCriticalSection(&opt_thread_reuse_lock);
	t = tp->pt_handle;
	memset(tp, 0, sizeof(opt_thread_t));
	tp->pt_handle = t;
	tp->pt_handle.x++;
	tp->prev_reuse = OPT_THREAD_REUSE_EMPTY;
	if (OPT_THREAD_REUSE_EMPTY != opt_thread_reuse_bottom)
		opt_thread_reuse_bottom->prev_reuse = tp;
	else
		opt_thread_reuse_top = tp;
	opt_thread_reuse_bottom = tp;
	LeaveCriticalSection(&opt_thread_reuse_lock);
}

static odr_pthread_t
opt_new(void)
{
	odr_pthread_t t;
	odr_pthread_t nil = { NULL, 0 };
	opt_thread_t * tp;

	t = opt_thread_reuse_pop();
	if (NULL != t.p) {
		tp = (opt_thread_t *) t.p;
	} else {
		tp = (opt_thread_t *)calloc(1, sizeof(opt_thread_t));
		if (tp == NULL)
			return nil;

		t.p = tp->pt_handle.p = tp;
		t.x = tp->pt_handle.x = 0;
	}

	tp->sched_priority = THREAD_PRIORITY_NORMAL;
	tp->detach_state = PTHREAD_CREATE_JOINABLE;
	tp->cancel_state = PTHREAD_CANCEL_ENABLE;
	tp->cancel_lock = PTHREAD_MUTEX_INITIALIZER;
	tp->thread_lock = PTHREAD_MUTEX_INITIALIZER;
	tp->cancel_event = CreateEvent(0, (int)TRUE, (int)FALSE, NULL);
	if (tp->cancel_event == NULL) {
		assert(0 == 1);
	}

	return t;
}

static int
odr_opt_initialize(void)
{

	if (opt_initialized) {
		return (1);
	}

	opt_initialized = 1;

	if ((ODR_pthread_key_create(&opt_self_thread_key, NULL) != 0) ||
	    (ODR_pthread_key_create(&opt_cleanup_key, NULL) != 0))
		assert(0 == 1);

	InitializeCriticalSection(&opt_thread_reuse_lock);
	InitializeCriticalSection(&opt_mutex_test_init_lock);
	InitializeCriticalSection(&opt_cond_list_lock);
	InitializeCriticalSection(&opt_cond_test_init_lock);
	return (opt_initialized);
}

static void
ODR_pthread_win32_process_attach_np(void)
{
	BOOL result = TRUE;

	result = odr_opt_initialize();
}

void
ODR_pthread_process_attach(void)
{

	ODR_pthread_win32_process_attach_np();
}

int
ODR_pthread_setspecific(odr_pthread_key_t key, void *value)
{
	odr_pthread_t self;
	int result = 0;

	if (key != opt_self_thread_key) {
		self = ODR_pthread_self();
		assert(self.p != NULL);
	} else {
		opt_thread_t *sp;

		sp = (opt_thread_t *)
		    ODR_pthread_getspecific(opt_self_thread_key);
		if (sp == NULL) {
			assert(value != NULL);
			self = *((odr_pthread_t *)value);
		} else
			self = sp->pt_handle;
	}

	result = 0;

	if (key != NULL) {
		if (self.p != NULL && key->destructor != NULL &&
		    value != NULL) {
			assert(0 == 1);
		}
		if (result == 0) {
			if (!TlsSetValue(key->key, (LPVOID) value)) {
				assert(0 == 1);
			}
		}
	}
	return (result);
}

int
ODR_pthread_key_create(odr_pthread_key_t *key, void (*destructor)(void *))
{
	int result = 0;
	odr_pthread_key_t newkey;

	newkey = (odr_pthread_key_t)calloc(1, sizeof(*newkey));
	assert(newkey != NULL);
	newkey->key = TlsAlloc();
	assert(newkey->key != TLS_OUT_OF_INDEXES);
	assert(destructor == NULL);

	*key = newkey;
	return (result);
}

void *
ODR_pthread_getspecific(odr_pthread_key_t key)
{
	void * ptr;

	if (key == NULL) {
		ptr = NULL;
	} else {
		int lasterror = GetLastError();
		int lastWSAerror = WSAGetLastError();

		ptr = TlsGetValue(key->key);

		SetLastError(lasterror);
		WSASetLastError(lastWSAerror);
	}
	return (ptr);
}

odr_pthread_t
ODR_pthread_self(void)
{
	odr_pthread_t self;
	odr_pthread_t nil = { NULL, 0 };
	opt_thread_t * sp;

	sp = (opt_thread_t *)ODR_pthread_getspecific(opt_self_thread_key);
	if (sp != NULL) {
		self = sp->pt_handle;
	} else {
		self = opt_new();
		sp = (opt_thread_t *) self.p;

		if (sp != NULL) {
			sp->implicit = 1;
			sp->detach_state = PTHREAD_CREATE_DETACHED;
			sp->thread = GetCurrentThreadId();

			if (!DuplicateHandle(GetCurrentProcess(),
				GetCurrentThread(),
				GetCurrentProcess(),
				&sp->threadH,
				0, FALSE, DUPLICATE_SAME_ACCESS)) {
				assert(0 == 1);
			}

			sp->sched_priority = GetThreadPriority(sp->threadH);

			ODR_pthread_setspecific(opt_self_thread_key,
			    (void *)sp);
		}
	}

	return (self);
}

int
ODR_pthread_equal(odr_pthread_t t1, odr_pthread_t t2)
{
	int result;

	result = ( t1.p == t2.p && t1.x == t2.x );

	return (result);
}

static int
sem_init(sem_t *sem, int pshared, unsigned int value)
{
	int result = 0;
	sem_t s = NULL;

	if (pshared != 0) {
		result = ODR_EPERM;
	} else if (value > (unsigned int)INT_MAX) {
		result = ODR_EINVAL;
	} else {
		s = (sem_t)calloc(1, sizeof(*s));

		if (NULL == s) {
			result = ODR_ENOMEM;
		} else {
			s->value = value;
			if (ODR_pthread_mutex_init(&s->lock, NULL) == 0) {
				if ((s->sem = CreateSemaphore(NULL,
				    (long) 0, (long) INT_MAX,
				    NULL)) == 0) {
					(void)ODR_pthread_mutex_destroy(&s->lock);
					result = ODR_ENOSPC;
				}
			} else
				result = ODR_ENOSPC;

			if (result != 0)
				free(s);
		}
	}

	if (result != 0)
		return (result);

	*sem = s;

	return 0;

}

static int
sem_destroy(sem_t * sem)
{
	int result = 0;
	sem_t s = NULL;

	if (sem == NULL || *sem == NULL) {
		result = ODR_EINVAL;
	} else {
		s = *sem;

		if ((result = ODR_pthread_mutex_lock(&s->lock)) == 0) {
			if (s->value < 0) {
				(void)ODR_pthread_mutex_unlock(&s->lock);
				result = ODR_EBUSY;
			} else {
				if (!CloseHandle(s->sem)) {
					(void)ODR_pthread_mutex_unlock(&s->lock);
					result = ODR_EINVAL;
				} else {
					*sem = NULL;
					s->value = INT_MAX;

					(void)ODR_pthread_mutex_unlock(&s->lock);

					do {
						Sleep(0);
					} while (ODR_pthread_mutex_destroy(&s->lock) == ODR_EBUSY);
				}
			}
		}
	}

	if (result != 0)
		return (result);
	free(s);
	return 0;
}

int
ODR_pthread_cond_init(odr_pthread_cond_t *cond,
    const odr_pthread_condattr_t *attr)
{
	int result;
	odr_pthread_cond_t cv = NULL;

	if (cond == NULL)
		return (ODR_EINVAL);

	if ((attr != NULL && *attr != NULL) &&
	    ((*attr)->pshared == PTHREAD_PROCESS_SHARED)) {
		result = ODR_ENOSYS;
		goto DONE;
	}

	cv = (odr_pthread_cond_t)calloc(1, sizeof(*cv));
	if (cv == NULL) {
		result = ODR_ENOMEM;
		goto DONE;
	}

	cv->n_waiters_blocked = 0;
	cv->n_waiters_to_unblock = 0;
	cv->n_waiters_gone = 0;

	result = sem_init(&(cv->sem_block_lock), 0, 1);
	if (result != 0)
		goto FAIL0;

	result = sem_init(&(cv->sem_block_queue), 0, 0);
	if (result != 0)
		goto FAIL1;

	if ((result = ODR_pthread_mutex_init(&(cv->mtx_unblock_lock), 0)) != 0)
		goto FAIL2;

	result = 0;

	goto DONE;

FAIL2:
	(void)sem_destroy(&(cv->sem_block_queue));

FAIL1:
	(void)sem_destroy(&(cv->sem_block_lock));

FAIL0:
	(void)free(cv);
	cv = NULL;

DONE:
	if (0 == result) {
		EnterCriticalSection(&opt_cond_list_lock);

		cv->next = NULL;
		cv->prev = opt_cond_list_tail;

		if (opt_cond_list_tail != NULL) {
			opt_cond_list_tail->next = cv;
		}

		opt_cond_list_tail = cv;

		if (opt_cond_list_head == NULL) {
			opt_cond_list_head = cv;
		}

		LeaveCriticalSection(&opt_cond_list_lock);
	}

	*cond = cv;

	return result;
}

static int
opt_semwait(sem_t * sem)
{
	int result = 0;
	sem_t s = *sem;

	if (s == NULL) {
		result = ODR_EINVAL;
	} else {
		if ((result = ODR_pthread_mutex_lock(&s->lock)) == 0) {
			int v = --s->value;

			(void)ODR_pthread_mutex_unlock(&s->lock);

			if (v < 0) {
				if (WaitForSingleObject(s->sem, INFINITE) ==
				    WAIT_OBJECT_0)
					return 0;
			} else
				return 0;
		}
	}
	if (result != 0)
		return (result);
	return 0;
}

static int
sem_post_multiple(sem_t * sem, int count)
{
	int result = 0;
	long waiters;
	sem_t s = *sem;

	if (s == NULL || count <= 0) {
		result = ODR_EINVAL;
	} else if ((result = ODR_pthread_mutex_lock(&s->lock)) == 0) {
		if (*sem == NULL) {
			(void)ODR_pthread_mutex_unlock(&s->lock);
			return (ODR_EINVAL);
		}

		if (s->value <= (INT_MAX - count)) {
			waiters = -s->value;
			s->value += count;
			if (waiters > 0) {
				if (ReleaseSemaphore(s->sem,
				    (waiters <= count) ? waiters : count, 0)) {
					/* XXX */
				} else {
					s->value -= count;
					result = ODR_EINVAL;
				}
			}
		} else {
			result = ODR_ERANGE;
		}
		(void)ODR_pthread_mutex_unlock(&s->lock);
	}
	if (result != 0)
		return (result);
	return 0;
}

static int
opt_cond_unblock(odr_pthread_cond_t * cond, int unblockAll)
{
	int result;
	odr_pthread_cond_t cv;
	int nSignalsToIssue;

	if (cond == NULL || *cond == NULL)
		return (ODR_EINVAL);

	cv = *cond;

	if (cv == PTHREAD_COND_INITIALIZER)
		return (0);

	if ((result = ODR_pthread_mutex_lock(&(cv->mtx_unblock_lock))) != 0)
		return result;

	if (0 != cv->n_waiters_to_unblock) {
		if (0 == cv->n_waiters_blocked)
			return ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock));
		if (unblockAll) {
			cv->n_waiters_to_unblock +=
			    (nSignalsToIssue = cv->n_waiters_blocked);
			cv->n_waiters_blocked = 0;
		} else {
			nSignalsToIssue = 1;
			cv->n_waiters_to_unblock++;
			cv->n_waiters_blocked--;
		}
	} else if (cv->n_waiters_blocked > cv->n_waiters_gone) {
		result = opt_semwait(&(cv->sem_block_lock));
		if (result != 0) {
			(void)ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock));
			return result;
		}
		if (0 != cv->n_waiters_gone) {
			cv->n_waiters_blocked -= cv->n_waiters_gone;
			cv->n_waiters_gone = 0;
		}
		if (unblockAll) {
			nSignalsToIssue = cv->n_waiters_to_unblock =
			    cv->n_waiters_blocked;
			cv->n_waiters_blocked = 0;
		} else {
			nSignalsToIssue = cv->n_waiters_to_unblock = 1;
			cv->n_waiters_blocked--;
		}
	} else
		return ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock));

	if ((result = ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock))) == 0)
		result = sem_post_multiple(&(cv->sem_block_queue),
		    nSignalsToIssue);

	return (result);
}

int
ODR_pthread_cond_signal(odr_pthread_cond_t *cond)
{

	return (opt_cond_unblock(cond, 0));
}

static void
opt_cond_wait_cleanup(void *args)
{
	opt_cond_wait_cleanup_args_t *cleanup_args =
		(opt_cond_wait_cleanup_args_t *) args;
	odr_pthread_cond_t cv = cleanup_args->cv;
	int *result_ptr = cleanup_args->result_ptr;
	int nSignalsWasLeft;
	int result;

	if ((result = ODR_pthread_mutex_lock(&(cv->mtx_unblock_lock))) != 0) {
		*result_ptr = result;
		return;
	}

	if (0 != (nSignalsWasLeft = cv->n_waiters_to_unblock)) {
		--(cv->n_waiters_to_unblock);
	} else if (INT_MAX / 2 == ++(cv->n_waiters_gone)) {
		*result_ptr = opt_semwait(&(cv->sem_block_lock));
		if (*result_ptr != 0) {
			return;
		}
		cv->n_waiters_blocked -= cv->n_waiters_gone;
		*result_ptr = sem_post(&(cv->sem_block_lock));
		if (*result_ptr != 0) {
			return;
		}
		cv->n_waiters_gone = 0;
	}

	if ((result = ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock))) != 0) {
		*result_ptr = result;
		return;
	}

	if (nSignalsWasLeft == 1) {
		*result_ptr = sem_post(&(cv->sem_block_lock));
		if (*result_ptr != 0)
			return;
	}
	if ((result = ODR_pthread_mutex_lock(cleanup_args->mutex_ptr)) != 0)
		*result_ptr = result;
}

static void
opt_push_cleanup(opt_cleanup_t * cleanup, opt_cleanup_callback_t routine,
    void *arg)
{
	cleanup->routine = routine;
	cleanup->arg = arg;
	cleanup->prev =
	    (opt_cleanup_t *)ODR_pthread_getspecific(opt_cleanup_key);

	ODR_pthread_setspecific(opt_cleanup_key, (void *)cleanup);
}

static opt_cleanup_t *
opt_pop_cleanup(int execute)
{
	opt_cleanup_t *cleanup;

	cleanup = (opt_cleanup_t *) ODR_pthread_getspecific(opt_cleanup_key);
	if (cleanup != NULL) {
		if (execute && (cleanup->routine != NULL))
			(*cleanup->routine)(cleanup->arg);
		ODR_pthread_setspecific(opt_cleanup_key,
		    (void *) cleanup->prev);
	}

	return (cleanup);
}

static int
sem_post(sem_t *sem)
{
	int result = 0;
	sem_t s = *sem;

	if (s == NULL) {
		result = ODR_EINVAL;
	} else if ((result = ODR_pthread_mutex_lock(&s->lock)) == 0) {
		if (*sem == NULL) {
			(void)ODR_pthread_mutex_unlock(&s->lock);
			return (ODR_EINVAL);
		}

		if (s->value < INT_MAX) {
			if (++s->value <= 0 &&
			    !ReleaseSemaphore(s->sem, 1, NULL)) {
				s->value--;
				result = ODR_EINVAL;
			}
		} else {
			result = ODR_ERANGE;
		}

		(void)ODR_pthread_mutex_unlock(&s->lock);
	}

	if (result != 0)
		return (result);

	return 0;
}

static void
opt_sem_wait_cleanup(void * sem)
{

	assert(0 == 1);
}

static void
opt_throw(DWORD exception)
{

	assert(0 == 1);
}

static void
ODR_pthread_testcancel(void)
{
	odr_pthread_t self = ODR_pthread_self();
	opt_thread_t * sp = (opt_thread_t *) self.p;

	if (sp == NULL)
		return;

	if (sp->state != PTHREAD_STATE_CANCEL_PENDING)
		return;

	(void)ODR_pthread_mutex_lock(&sp->cancel_lock);
	if (sp->cancel_state != PTHREAD_CANCEL_DISABLE) {
		ResetEvent(sp->cancel_event);
		sp->state = PTHREAD_STATE_CANCELING;
		assert(0 == 1);
		sp->cancel_state = PTHREAD_CANCEL_DISABLE;
		(void)ODR_pthread_mutex_unlock(&sp->cancel_lock);
		opt_throw(OPT_EPS_CANCEL);
	}
	(void)ODR_pthread_mutex_unlock(&sp->cancel_lock);
}

static int
opt_cancelable_wait(HANDLE waitHandle, DWORD timeout)
{
	int result;
	odr_pthread_t self;
	opt_thread_t * sp;
	HANDLE handles[2];
	DWORD nHandles = 1;
	DWORD status;

	handles[0] = waitHandle;

	self = ODR_pthread_self();
	sp = (opt_thread_t *) self.p;

	if (sp != NULL) {
		if (sp->cancel_state == PTHREAD_CANCEL_ENABLE) {
			if ((handles[1] = sp->cancel_event) != NULL) {
				nHandles++;
			}
		}
	} else {
		handles[1] = NULL;
	}

	status = WaitForMultipleObjects(nHandles, handles, FALSE,
	    timeout);

	switch (status - WAIT_OBJECT_0) {
	case 0:
		result = 0;
		break;
	case 1:
		ResetEvent(handles[1]);

		if (sp != NULL) {
			(void)ODR_pthread_mutex_lock(&sp->cancel_lock);
			if (sp->state < PTHREAD_STATE_CANCELING) {
				sp->state = PTHREAD_STATE_CANCELING;
				sp->cancel_state = PTHREAD_CANCEL_DISABLE;
				(void)ODR_pthread_mutex_unlock(&sp->cancel_lock);
				opt_throw(OPT_EPS_CANCEL);
			}
			(void)ODR_pthread_mutex_unlock(&sp->cancel_lock);
		}
		result = ODR_EINVAL;
		break;
	default:
		if (status == WAIT_TIMEOUT) {
			result = ODR_ETIMEDOUT;
		} else {
			result = ODR_EINVAL;
		}
		break;
	}
	return (result);
}

static int
odr_pthread_cancelable_wait(HANDLE waitHandle)
{

	return (opt_cancelable_wait(waitHandle, INFINITE));
}

static int
odr_pthread_cancelable_timedwait(HANDLE waitHandle, DWORD timeout)
{

	return (opt_cancelable_wait(waitHandle, timeout));
}

static int
sem_wait(sem_t *sem)
{
	int result = 0;
	sem_t s = *sem;

	ODR_pthread_testcancel();

	if (s == NULL) {
		result = ODR_EINVAL;
	} else {
		if ((result = ODR_pthread_mutex_lock(&s->lock)) == 0) {
			int v;

			if (*sem == NULL) {
				(void)ODR_pthread_mutex_unlock(&s->lock);
				return (ODR_EINVAL);
			}

			v = --s->value;
			(void)ODR_pthread_mutex_unlock(&s->lock);

			if (v < 0) {
				ODR_pthread_cleanup_push(opt_sem_wait_cleanup,
				    (void *) s);
				result = odr_pthread_cancelable_wait(s->sem);
				ODR_pthread_cleanup_pop(result);
			}
		}
	}

	if (result != 0)
		return (result);
	return (0);
}

static void
opt_sem_timedwait_cleanup (void *args)
{
	sem_timedwait_cleanup_args_t * a = (sem_timedwait_cleanup_args_t *)args;
	sem_t s = a->sem;

	if (ODR_pthread_mutex_lock(&s->lock) == 0) {
		if (WaitForSingleObject(s->sem, 0) == WAIT_OBJECT_0) {
			*(a->result_ptr) = 0;
		} else {
			s->value++;
		}
		(void)ODR_pthread_mutex_unlock(&s->lock);
	}
}

#define OPT_TIMESPEC_TO_FILETIME_OFFSET \
	  (((LONGLONG) 27111902 << 32) + (LONGLONG)3577643008 )

static void
opt_filetime_to_timespec(const FILETIME * ft, struct odr_timespec *ts)
{

	ts->tv_sec =
	  (int) ((*(LONGLONG *) ft - OPT_TIMESPEC_TO_FILETIME_OFFSET) /
	      10000000);
	ts->tv_nsec =
	    (int) ((*(LONGLONG *) ft - OPT_TIMESPEC_TO_FILETIME_OFFSET -
		((LONGLONG) ts->tv_sec * (LONGLONG) 10000000)) * 100);
}

static DWORD
opt_relmillisecs(const struct odr_timespec *abstime)
{
	const int64_t NANOSEC_PER_MILLISEC = 1000000;
	const int64_t MILLISEC_PER_SEC = 1000;
	DWORD milliseconds;
	int64_t tmpAbsMilliseconds;
	int64_t tmpCurrMilliseconds;
	struct odr_timespec currSysTime;
	FILETIME ft;
	SYSTEMTIME st;

	tmpAbsMilliseconds =  (int64_t)abstime->tv_sec * MILLISEC_PER_SEC;
	tmpAbsMilliseconds += ((int64_t)abstime->tv_nsec +
	    (NANOSEC_PER_MILLISEC/2)) / NANOSEC_PER_MILLISEC;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	
	opt_filetime_to_timespec(&ft, &currSysTime);

	tmpCurrMilliseconds = (int64_t)currSysTime.tv_sec * MILLISEC_PER_SEC;
	tmpCurrMilliseconds += ((int64_t)currSysTime.tv_nsec +
	    (NANOSEC_PER_MILLISEC/2)) / NANOSEC_PER_MILLISEC;

	if (tmpAbsMilliseconds > tmpCurrMilliseconds) {
		milliseconds = (DWORD) (tmpAbsMilliseconds -
		    tmpCurrMilliseconds);
		if (milliseconds == INFINITE) {
			milliseconds--;
		}
	} else {
		milliseconds = 0;
	}

	return milliseconds;
}

static int
sem_timedwait(sem_t *sem, const struct odr_timespec *abstime)
{
	int result = 0;
	sem_t s = *sem;

	ODR_pthread_testcancel();

	if (sem == NULL) {
		result = ODR_EINVAL;
	} else {
		DWORD milliseconds;

		if (abstime == NULL) {
			milliseconds = INFINITE;
		} else {
			milliseconds = opt_relmillisecs(abstime);
		}

		if ((result = ODR_pthread_mutex_lock(&s->lock)) == 0) {
			int v;

			if (*sem == NULL) {
				(void)ODR_pthread_mutex_unlock(&s->lock);
				return (ODR_EINVAL);
			}

			v = --s->value;
			(void)ODR_pthread_mutex_unlock(&s->lock);

			if (v < 0) {
				sem_timedwait_cleanup_args_t cleanup_args;

				cleanup_args.sem = s;
				cleanup_args.result_ptr = &result;

				ODR_pthread_cleanup_push(opt_sem_timedwait_cleanup,
				    (void *) &cleanup_args);
				result = odr_pthread_cancelable_timedwait(s->sem,
				    milliseconds);
				ODR_pthread_cleanup_pop(result);
			}
		}
	}
	if (result != 0)
		return (result);
	return 0;
}

static int
opt_cond_check_need_init(odr_pthread_cond_t * cond)
{
	int result = 0;

	EnterCriticalSection(&opt_cond_test_init_lock);

	if (*cond == PTHREAD_COND_INITIALIZER)
		result = ODR_pthread_cond_init(cond, NULL);
	else if (*cond == NULL) {
		result = EINVAL;
	}
	LeaveCriticalSection(&opt_cond_test_init_lock);
	return (result);
}

static int
opt_cond_timedwait(odr_pthread_cond_t * cond, odr_pthread_mutex_t *mutex,
    const struct odr_timespec *abstime)
{
	int result = 0;
	odr_pthread_cond_t cv;
	opt_cond_wait_cleanup_args_t cleanup_args;

	if (cond == NULL || *cond == NULL)
		return ODR_EINVAL;

	if (*cond == PTHREAD_COND_INITIALIZER)
		result = opt_cond_check_need_init(cond);

	if (result != 0 && result != ODR_EBUSY)
		return result;

	cv = *cond;

	result = sem_wait(&(cv->sem_block_lock));
	if (result != 0)
		return (result);

	++(cv->n_waiters_blocked);

	result = sem_post(&(cv->sem_block_lock));
	if (result != 0)
		return (result);

	cleanup_args.mutex_ptr = mutex;
	cleanup_args.cv = cv;
	cleanup_args.result_ptr = &result;

	ODR_pthread_cleanup_push(opt_cond_wait_cleanup,
	    (void *)&cleanup_args);

	if ((result = ODR_pthread_mutex_unlock(mutex)) == 0) {
		result = sem_timedwait(&(cv->sem_block_queue), abstime);
	}

	ODR_pthread_cleanup_pop(1);

	return result;
}

int
ODR_pthread_cond_wait(odr_pthread_cond_t *cond, odr_pthread_mutex_t *mutex)
{

	return (opt_cond_timedwait(cond, mutex, NULL));
}

int
ODR_pthread_cond_timedwait(odr_pthread_cond_t *cond, odr_pthread_mutex_t *mutex,
    const struct odr_timespec *abstime)
{

	if (abstime == NULL)
		return ODR_EINVAL;
	return (opt_cond_timedwait(cond, mutex, abstime));
}

int
ODR_pthread_cond_broadcast(odr_pthread_cond_t *cond)
{

	return (opt_cond_unblock(cond, TRUE));
}

int
ODR_pthread_cond_destroy(odr_pthread_cond_t *cond)
{
	odr_pthread_cond_t cv;
	int result = 0, result1 = 0, result2 = 0;
	int ret;

	if (cond == NULL || *cond == NULL) {
		return (ODR_EINVAL);
	}

	if (*cond != PTHREAD_COND_INITIALIZER) {
		EnterCriticalSection(&opt_cond_list_lock);

		cv = *cond;

		ret = sem_wait(&(cv->sem_block_lock));
		if (ret != 0) {
			return ret;
		}

		if ((result = ODR_pthread_mutex_trylock(&(cv->mtx_unblock_lock))) != 0) {
			(void)sem_post(&(cv->sem_block_lock));
			return result;
		}

		if (cv->n_waiters_blocked > cv->n_waiters_gone) {
			ret = sem_post(&(cv->sem_block_lock));
			if (ret != 0) {
				result = ret;
			}
			result1 = ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock));
			result2 = ODR_EBUSY;
		} else {
			*cond = NULL;

			ret = sem_destroy(&(cv->sem_block_lock));
			if (ret != 0) {
				result = ret;
			}
			ret = sem_destroy(&(cv->sem_block_queue));
			if (ret != 0) {
				result1 = errno;
			}
			if ((result2 = ODR_pthread_mutex_unlock(&(cv->mtx_unblock_lock))) == 0) {
				result2 = ODR_pthread_mutex_destroy(&(cv->mtx_unblock_lock));
			}

			if (opt_cond_list_head == cv) {
				opt_cond_list_head = cv->next;
			} else {
				cv->prev->next = cv->next;
			}

			if (opt_cond_list_tail == cv) {
				opt_cond_list_tail = cv->prev;
			} else {
				cv->next->prev = cv->prev;
			}

			(void)free(cv);
		}

		LeaveCriticalSection(&opt_cond_list_lock);
	} else {
		EnterCriticalSection(&opt_cond_test_init_lock);

		if (*cond == PTHREAD_COND_INITIALIZER) {
			*cond = NULL;
		} else {
			result = ODR_EBUSY;
		}

		LeaveCriticalSection(&opt_cond_test_init_lock);
	}

	return ((result != 0) ? result : ((result1 != 0) ? result1 : result2));
}

/*****************************************************************************/

int
ODR_pthread_mutex_init(odr_pthread_mutex_t *mutex,
    const odr_pthread_mutexattr_t *attr)
{
	int result = 0;
	odr_pthread_mutex_t mx;

	if (mutex == NULL)
		return (ODR_EINVAL);

	if (attr != NULL
	    && *attr != NULL && (*attr)->pshared == PTHREAD_PROCESS_SHARED) {
		return (ODR_ENOSYS);
	}

	mx = (odr_pthread_mutex_t)calloc(1, sizeof(*mx));
	if (mx == NULL) {
		result = ODR_ENOMEM;
	} else {
		mx->lock_idx = 0;
		mx->recursive_count = 0;
		mx->kind = (attr == NULL || *attr == NULL
		    ? PTHREAD_MUTEX_DEFAULT : (*attr)->kind);
		mx->ownerThread.p = NULL;

		mx->event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (0 == mx->event) {
			result = ODR_ENOSPC;
			free(mx);
			mx = NULL;
		}
	}

	*mutex = mx;

	return (result);
}

static int
opt_mutex_check_need_init(odr_pthread_mutex_t *mutex)
{
	int result = 0;
	odr_pthread_mutex_t mtx;

	EnterCriticalSection(&opt_mutex_test_init_lock);

	mtx = *mutex;

	if (mtx == PTHREAD_MUTEX_INITIALIZER) {
		result = ODR_pthread_mutex_init(mutex, NULL);
	} else if (mtx == PTHREAD_RECURSIVE_MUTEX_INITIALIZER) {
		result = ODR_pthread_mutex_init(mutex,
		    &opt_recursive_mutexattr);
	} else if (mtx == PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) {
		result = ODR_pthread_mutex_init(mutex,
		    &opt_errorcheck_mutexattr);
	} else if (mtx == NULL) {
		result = ODR_EINVAL;
	}
	LeaveCriticalSection(&opt_mutex_test_init_lock);
	return (result);
}

int
ODR_pthread_mutex_lock(odr_pthread_mutex_t *mutex)
{
	int result = 0;
	odr_pthread_mutex_t mx;

	if (*mutex == NULL)
		return (ODR_EINVAL);

	if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) {
		if ((result = opt_mutex_check_need_init(mutex)) != 0)
			return (result);
	}

	mx = *mutex;

	if (mx->kind == PTHREAD_MUTEX_NORMAL) {
		if ((LONG)InterlockedExchange((LONG volatile *)&mx->lock_idx,
		    (LONG)1) != 0) {
			while ((LONG)InterlockedExchange(
			    (LONG volatile *)&mx->lock_idx, (LONG) -1) != 0) {
				if (WAIT_OBJECT_0 !=
				    WaitForSingleObject(mx->event, INFINITE)) {
					result = ODR_EINVAL;
					break;
				}
			}
		}
	} else {
		odr_pthread_t self = ODR_pthread_self();

		if ((LONG)InterlockedCompareExchange(
		    (LONG volatile *)&mx->lock_idx, (LONG) 1, (LONG) 0) == 0) {
			mx->recursive_count = 1;
			mx->ownerThread = self;
		} else {
			if (ODR_pthread_equal(mx->ownerThread, self)) {
				if (mx->kind == PTHREAD_MUTEX_RECURSIVE) {
					mx->recursive_count++;
				} else {
					result = ODR_EDEADLK;
				}
			} else {
				while ((LONG)InterlockedExchange(
				    (LONG volatile *)&mx->lock_idx,
				    (LONG) -1) != 0) {
					if (WAIT_OBJECT_0 !=
					    WaitForSingleObject(mx->event,
						INFINITE)) {
						result = ODR_EINVAL;
						break;
					}
				}

				if (result == 0) {
					mx->recursive_count = 1;
					mx->ownerThread = self;
				}
			}
		}
	}

	return (result);
}

int
ODR_pthread_mutex_unlock(odr_pthread_mutex_t *mutex)
{
	int result = 0;
	odr_pthread_mutex_t mx;

	mx = *mutex;

	if (mx >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
		return (ODR_EINVAL);
	if (mx->kind == PTHREAD_MUTEX_NORMAL) {
		LONG idx;

		idx = (LONG)InterlockedExchange((LONG volatile *)&mx->lock_idx,
		    (LONG) 0);
		if (idx != 0) {
			if (idx < 0) {
				if (SetEvent(mx->event) == 0)
					result = ODR_EINVAL;
			}
		} else {
			result = ODR_EPERM;
		}
	} else {
		if (ODR_pthread_equal(mx->ownerThread, ODR_pthread_self())) {
			if (mx->kind != PTHREAD_MUTEX_RECURSIVE ||
			    --mx->recursive_count == 0) {
				mx->ownerThread.p = NULL;

				if ((LONG)InterlockedExchange(
				    (LONG volatile *)&mx->lock_idx,
				    (LONG) 0) < 0) {
					if (SetEvent(mx->event) == 0)
						result = ODR_EINVAL;
				}
			}
		} else
			result = ODR_EPERM;
	}
	return (result);
}

int
ODR_pthread_mutex_trylock(odr_pthread_mutex_t *mutex)
{
	int result = 0;
	odr_pthread_mutex_t mx;

	if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) {
		if ((result = opt_mutex_check_need_init(mutex)) != 0)
			return (result);
	}

	mx = *mutex;

	if (0 == (LONG)InterlockedCompareExchange(
	    (LONG volatile *) &mx->lock_idx, (LONG)1, (LONG)0)) {
		if (mx->kind != PTHREAD_MUTEX_NORMAL) {
			mx->recursive_count = 1;
			mx->ownerThread = ODR_pthread_self();
		}
	} else {
		if (mx->kind == PTHREAD_MUTEX_RECURSIVE &&
		    ODR_pthread_equal(mx->ownerThread, ODR_pthread_self())) {
			mx->recursive_count++;
		} else {
			result = ODR_EBUSY;
		}
	}

	return (result);
}

int
ODR_pthread_mutex_destroy(odr_pthread_mutex_t *mutex)
{
	int result = 0;
	odr_pthread_mutex_t mx;

	if (*mutex < PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) {
		mx = *mutex;

		result = ODR_pthread_mutex_trylock(&mx);

		if (result == 0) {
			if (mx->kind != PTHREAD_MUTEX_RECURSIVE ||
			    1 == mx->recursive_count) {
				*mutex = NULL;

				result = ODR_pthread_mutex_unlock(&mx);
				if (result == 0) {
					if (!CloseHandle(mx->event)) {
						*mutex = mx;
						result = ODR_EINVAL;
					} else {
						free(mx);
					}
				} else {
					*mutex = mx;
				}
			} else {
				mx->recursive_count--;
				result = ODR_EBUSY;
			}
		}
	} else {
		EnterCriticalSection(&opt_mutex_test_init_lock);
		if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) {
			*mutex = NULL;
		} else {
			result = ODR_EBUSY;
		}
		LeaveCriticalSection(&opt_mutex_test_init_lock);
	}

	return (result);
}

int
ODR_pthread_mutexattr_init(odr_pthread_mutexattr_t *attr)
{
	int result = 0;
	odr_pthread_mutexattr_t ma;

	ma = (odr_pthread_mutexattr_t)calloc(1, sizeof(*ma));
	if (ma == NULL)
		return (ODR_ENOMEM);
	ma->pshared = PTHREAD_PROCESS_PRIVATE;
	ma->kind = PTHREAD_MUTEX_DEFAULT;
	*attr = ma;

	return (result);
}

int
ODR_pthread_mutexattr_settype(odr_pthread_mutexattr_t *attr, int kind)
{
	int result = 0;

	if ((attr != NULL && *attr != NULL)) {
		switch (kind) {
		case PTHREAD_MUTEX_FAST_NP:
		case PTHREAD_MUTEX_RECURSIVE_NP:
		case PTHREAD_MUTEX_ERRORCHECK_NP:
			(*attr)->kind = kind;
			break;
		default:
			result = ODR_EINVAL;
			break;
		}
	} else
		result = ODR_EINVAL;

	return (result);
}

int
ODR_pthread_mutexattr_destroy(odr_pthread_mutexattr_t *attr)
{
	int result = 0;

	if (attr == NULL || *attr == NULL) {
		result = ODR_EINVAL;
	} else {
		odr_pthread_mutexattr_t ma = *attr;

		*attr = NULL;
		free(ma);
	}

	return (result);
}

static void
opt_thread_destroy(odr_pthread_t thread)
{
	opt_thread_t * tp = (opt_thread_t *)thread.p;
	opt_thread_t threadCopy;

	if (tp != NULL) {
		memcpy(&threadCopy, tp, sizeof(threadCopy));

		opt_thread_reuse_push(thread);

		if (threadCopy.cancel_event != NULL)
			CloseHandle(threadCopy.cancel_event);

		(void)ODR_pthread_mutex_destroy(&threadCopy.cancel_lock);
		(void)ODR_pthread_mutex_destroy(&threadCopy.thread_lock);

		if (threadCopy.threadH != 0)
			CloseHandle(threadCopy.threadH);
	}
}

static void
opt_call_user_destroy_routines(odr_pthread_t thread)
{
	thread_key_assoc *assoc;
	int assocs_remaining;
	int iterations = 0;
	opt_thread_t *sp = (opt_thread_t *)thread.p;

	if (sp == NULL)
		return;

	do {
		assocs_remaining = 0;
		iterations++;

		(void)ODR_pthread_mutex_lock(&(sp->thread_lock));
		sp->next_assoc = sp->keys;
		(void)ODR_pthread_mutex_unlock(&(sp->thread_lock));

		for (;;) {
			void * value;
			odr_pthread_key_t k;
			void (*destructor) (void *);

			(void)ODR_pthread_mutex_lock(&(sp->thread_lock));

			if ((assoc = (thread_key_assoc *)sp->next_assoc) == NULL) {
				ODR_pthread_mutex_unlock(&(sp->thread_lock));
				break;
			} else {
				if (ODR_pthread_mutex_trylock(&(assoc->key->keylock)) == ODR_EBUSY) {
					ODR_pthread_mutex_unlock(&(sp->thread_lock));
					Sleep(1);
					continue;
				}
			}

			sp->next_assoc = assoc->next_key;

			k = assoc->key;
			destructor = k->destructor;
			value = TlsGetValue(k->key);
			TlsSetValue(k->key, NULL);

			if (value != NULL &&
			    iterations <= PTHREAD_DESTRUCTOR_ITERATIONS) {
				(void)ODR_pthread_mutex_unlock(&(sp->thread_lock));
				(void)ODR_pthread_mutex_unlock(&(k->keylock));
				assocs_remaining++;
				destructor(value);
			} else {
				assert(0 == 1);
			}
		}
	} while (assocs_remaining);
}

static BOOL
pthread_win32_thread_detach_np(void)
{

	if (opt_initialized) {
		opt_thread_t *sp =
		    (opt_thread_t *)ODR_pthread_getspecific(opt_self_thread_key);

		if (sp != NULL) {
			opt_call_user_destroy_routines(sp->pt_handle);

			(void)ODR_pthread_mutex_lock(&sp->cancel_lock);
			sp->state = PTHREAD_STATE_LAST;
			(void)ODR_pthread_mutex_unlock(&sp->cancel_lock);

			if (sp->detach_state == PTHREAD_CREATE_DETACHED) {
				opt_thread_destroy(sp->pt_handle);
				TlsSetValue(opt_self_thread_key->key, NULL);
			}
		}
	}

	return (TRUE);
}

static unsigned __stdcall
opt_thread_start(void *vparams)
{
	odr_thread_params *params = (odr_thread_params *) vparams;
	odr_pthread_t self;
	opt_thread_t *sp;
	void *(*start)(void *);
	void *arg;
	void *status = NULL;
	int setjmp_rc;

	self = params->tid;
	sp = (opt_thread_t *) self.p;
	start = params->start;
	arg = params->arg;

	free(params);

	ODR_pthread_setspecific(opt_self_thread_key, sp);
	sp->state = PTHREAD_STATE_RUNNING;

	setjmp_rc = setjmp(sp->start_mark);

	if (0 == setjmp_rc) {
		status = sp->exit_status = (*start) (arg);
	} else {
		switch (setjmp_rc) {
		case OPT_EPS_CANCEL:
			status = sp->exit_status = PTHREAD_CANCELED;
			break;
		case OPT_EPS_EXIT:
			status = sp->exit_status;
			break;
		default:
			status = sp->exit_status = PTHREAD_CANCELED;
			break;
		}
	}

	(void)pthread_win32_thread_detach_np();
	_endthreadex((unsigned)status);
	return (unsigned) status;
}

int
ODR_pthread_create(odr_pthread_t *tid, const odr_pthread_attr_t *attr,
    void *(*start)(void *), void *arg)
{
	odr_pthread_t thread;
	opt_thread_t * tp;
	odr_pthread_attr_t a;
	HANDLE threadH = 0;
	int result = ODR_EAGAIN;
	int run = TRUE;
	odr_thread_params *parms = NULL;
	long stackSize;
	int priority;
	odr_pthread_t self;

	tid->x = 0;

	if (attr != NULL) {
		a = *attr;
	} else {
		a = NULL;
	}

	if ((thread = opt_new()).p == NULL)
		goto FAIL0;

	tp = (opt_thread_t *) thread.p;

	priority = tp->sched_priority;

	if ((parms = (odr_thread_params *)malloc(sizeof(*parms))) == NULL) 
		goto FAIL0;

	parms->tid = thread;
	parms->start = start;
	parms->arg = arg;

	if (a != NULL) {
		stackSize = a->stacksize;
		tp->detach_state = a->detachstate;
		priority = a->param.sched_priority;
		if (PTHREAD_INHERIT_SCHED == a->inheritsched) {
			self = ODR_pthread_self();
			priority = ((opt_thread_t *) self.p)->sched_priority;
		}
	} else {
		stackSize = PTHREAD_STACK_MIN;
	}

	tp->state = run ? PTHREAD_STATE_INITIAL : PTHREAD_STATE_SUSPENDED;
	tp->keys = NULL;
	tp->threadH = threadH = (HANDLE)_beginthreadex(
	    (void *) NULL,
	    (unsigned) stackSize,
	    opt_thread_start,
	    parms,
	    (unsigned)
	    CREATE_SUSPENDED,
	    (unsigned *) &(tp->thread));
	if (threadH != 0) {
		if (a != NULL) {
			assert(0 == 1);
		}
		if (run)
			ResumeThread(threadH);
	}

	result = (threadH != 0) ? 0 : ODR_EAGAIN;
FAIL0:
	if (result != 0) {
		opt_thread_destroy(thread);
		tp = NULL;

		if (parms != NULL)
			free(parms);
	} else
		*tid = thread;

	return (result);
}

void
ODR_pthread_free(odr_pthread_t thread)
{

	/* Do nothing */
}

int
ODR_pthread_join(odr_pthread_t thread, void **value_ptr)
{
	int result;
	odr_pthread_t self;
	opt_thread_t *tp = (opt_thread_t *)thread.p;

	EnterCriticalSection(&opt_thread_reuse_lock);

	if (NULL == tp || thread.x != tp->pt_handle.x) {
		result = ODR_ESRCH;
	} else if (PTHREAD_CREATE_DETACHED == tp->detach_state) {
		result = ODR_EINVAL;
	} else {
		result = 0;
	}

	LeaveCriticalSection(&opt_thread_reuse_lock);

	if (result == 0) {
		self = ODR_pthread_self();

		if (NULL == self.p) {
			result = ODR_ENOENT;
		} else if (ODR_pthread_equal(self, thread)) {
			result = ODR_EDEADLK;
		} else {
			result = odr_pthread_cancelable_wait(tp->threadH);

			if (0 == result) {
				if (value_ptr != NULL) {
					*value_ptr = tp->exit_status;
				}
				result = ODR_pthread_detach(thread);
			} else {
				result = ODR_ESRCH;
			}
		}
	}

	return (result);
}

int
ODR_pthread_detach(odr_pthread_t thread)
{
	int result;
	BOOL destroyIt = FALSE;
	opt_thread_t * tp = (opt_thread_t *) thread.p;

	EnterCriticalSection(&opt_thread_reuse_lock);
	if (NULL == tp || thread.x != tp->pt_handle.x) {
		result = ODR_ESRCH;
	} else if (PTHREAD_CREATE_DETACHED == tp->detach_state) {
		result = ODR_EINVAL;
	} else {
		result = 0;

		if (ODR_pthread_mutex_lock(&tp->cancel_lock) == 0) {
			if (tp->state != PTHREAD_STATE_LAST) {
				tp->detach_state = PTHREAD_CREATE_DETACHED;
			} else if (tp->detach_state != PTHREAD_CREATE_DETACHED) {
				destroyIt = TRUE;
			}
			(void)ODR_pthread_mutex_unlock(&tp->cancel_lock);
		} else {
			result = ODR_ESRCH;
		}
	}
	LeaveCriticalSection(&opt_thread_reuse_lock);

	if (result == 0) {
		if (destroyIt) {
			(void)WaitForSingleObject(tp->threadH, INFINITE);
			opt_thread_destroy(thread);
		}
	}

	return (result);
}

int
ODR_pthread_detach_self(void)
{

	return (ODR_pthread_detach(ODR_pthread_self()));
}

