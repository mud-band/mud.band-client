/*-
 * Copyright (c) 1983, 1988, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>

#include "vopt.h"

int	vopt_err = 1,		/* if error message should be printed */
	vopt_ind = 1,		/* index into parent argv vector */
	vopt_opt,		/* character checked for validity */
	vopt_reset;		/* reset getopt */
const char *vopt_arg;		/* argument associated with option */

#define	BADCH	(int)'?'
#define	BADARG	(int)':'
#define	EMSG	""

/*
 * getopt --
 *	Parse argc/argv argument vector.
 */
int
VOPT_get(int nargc, char * const nargv[], const char *ostr)
{
	static const char *place = EMSG;	/* option letter processing */
	char *oli;				/* option letter list index */

	if (vopt_reset || *place == 0) {	/* update scanning pointer */
		vopt_reset = 0;
		place = nargv[vopt_ind];
		if (vopt_ind >= nargc || *place++ != '-') {
			/* Argument is absent or is not an option */
			place = EMSG;
			return (-1);
		}
		vopt_opt = *place++;
		if (vopt_opt == '-' && *place == 0) {
			/* "--" => end of options */
			++vopt_ind;
			place = EMSG;
			return (-1);
		}
		if (vopt_opt == 0) {
			/* Solitary '-', treat as a '-' option
			   if the program (eg su) is looking for it. */
			place = EMSG;
			if (strchr(ostr, '-') == NULL)
				return (-1);
			vopt_opt = '-';
		}
	} else
		vopt_opt = *place++;

	/* See if option letter is one the caller wanted... */
	if (vopt_opt == ':' || (oli = strchr(ostr, vopt_opt)) == NULL) {
		if (*place == 0)
			++vopt_ind;
		if (vopt_err && *ostr != ':')
			(void)fprintf(stderr,
			    "illegal option -- %c\n", vopt_opt);
		return (BADCH);
	}

	/* Does this option need an argument? */
	if (oli[1] != ':') {
		/* don't need argument */
		vopt_arg = NULL;
		if (*place == 0)
			++vopt_ind;
	} else {
		/* Option-argument is either the rest of this argument or the
		   entire next argument. */
		if (*place)
			vopt_arg = place;
		else if (nargc > ++vopt_ind)
			vopt_arg = nargv[vopt_ind];
		else {
			/* option-argument absent */
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (vopt_err)
				(void)fprintf(stderr,
				    "option requires an argument -- %c\n",
				    vopt_opt);
			return (BADCH);
		}
		place = EMSG;
		++vopt_ind;
	}
	return (vopt_opt);			/* return option letter */
}

#define VOPT_LONG_FLAG_PERMUTE 0x01 /* permute non-options to the end of argv */
/* treat non-options as args to option "-1" */
#define VOPT_LONG_FLAG_ALLARGS 0x02
#define VOPT_LONG_FLAG_LONGONLY	0x04	/* operate as VOPT_long_only */

/* return values */
#define	VOPT_LONG_BADCH		(int)'?'
#define	VOPT_LONG_BADARG	((*options == ':') ? (int)':' : (int)'?')
#define	VOPT_LONG_INORDER 	(int)1

#define	VOPT_LONG_EMSG		""

static int	vopt_get_long_internal(int, char **, const char *,
		    const struct vopt_option *, int *, int);
static int	vopt_long_parse_options(char **, const char *,
		    const struct vopt_option *, int *, int, int);
static int	vopt_long_gcd(int, int);
static void	vopt_long_permute_args(int, int, int, char **);

/* option letter processing */
static const char *vopt_long_place = VOPT_LONG_EMSG;

/* XXX: set vopt_reset to 1 rather than these two */
static int vopt_long_nonopt_start = -1; /* first non option argument (for permute) */
static int vopt_long_nonopt_end = -1;   /* first option after non options (for permute) */

/*
 * Compute the greatest common divisor of a and b.
 */
static int
vopt_long_gcd(int a, int b)
{
	int c;

	c = a % b;
	while (c != 0) {
		a = b;
		b = c;
		c = a % b;
	}

	return (b);
}

/*
 * Exchange the block from vopt_long_nonopt_start to vopt_long_nonopt_end with the block
 * from vopt_long_nonopt_end to opt_end (keeping the same order of arguments
 * in each block).
 */
static void
vopt_long_permute_args(int pavopt_long_nonopt_start,
    int pavopt_long_nonopt_end, int opt_end, char **nargv)
{
	int cstart, cyclelen, i, j, ncycle, nnonopts, nopts, pos;
	char *swap;

	/*
	 * compute lengths of blocks and number and size of cycles
	 */
	nnonopts = pavopt_long_nonopt_end - pavopt_long_nonopt_start;
	nopts = opt_end - pavopt_long_nonopt_end;
	ncycle = vopt_long_gcd(nnonopts, nopts);
	cyclelen = (opt_end - pavopt_long_nonopt_start) / ncycle;

	for (i = 0; i < ncycle; i++) {
		cstart = pavopt_long_nonopt_end+i;
		pos = cstart;
		for (j = 0; j < cyclelen; j++) {
			if (pos >= pavopt_long_nonopt_end)
				pos -= nnonopts;
			else
				pos += nopts;
			swap = nargv[pos];
			/* LINTED const cast */
			((char **) nargv)[pos] = nargv[cstart];
			/* LINTED const cast */
			((char **)nargv)[cstart] = swap;
		}
	}
}

/*
 * vopt_long_parse_options --
 *	Parse long options in argc/argv argument vector.
 * Returns -1 if short_too is set and the option does not match long_options.
 */
static int
vopt_long_parse_options(char **nargv, const char *options,
    const struct vopt_option *long_options, int *idx, int short_too, int flags)
{
	const char *current_argv;
	char *has_equal;
	size_t current_argv_len;
	int i, match, exact_match, second_partial_match;

	current_argv = vopt_long_place;
	match = -1;
	exact_match = 0;
	second_partial_match = 0;

	vopt_ind++;

	if ((has_equal = strchr(current_argv, '=')) != NULL) {
		/* argument found (--option=arg) */
		current_argv_len = has_equal - current_argv;
		has_equal++;
	} else
		current_argv_len = strlen(current_argv);

	for (i = 0; long_options[i].name; i++) {
		/* find matching long option */
		if (strncmp(current_argv, long_options[i].name,
			current_argv_len))
			continue;

		if (strlen(long_options[i].name) == current_argv_len) {
			/* exact match */
			match = i;
			exact_match = 1;
			break;
		}
		/*
		 * If this is a known short option, don't allow
		 * a partial match of a single character.
		 */
		if (short_too && current_argv_len == 1)
			continue;

		if (match == -1)        /* first partial match */
			match = i;
		else if ((flags & VOPT_LONG_FLAG_LONGONLY) ||
		    long_options[i].has_arg !=
		    long_options[match].has_arg ||
		    long_options[i].flag != long_options[match].flag ||
		    long_options[i].val != long_options[match].val)
			second_partial_match = 1;
	}
	if (!exact_match && second_partial_match) {
		/* ambiguous abbreviation */
		vopt_opt = 0;
		return (VOPT_LONG_BADCH);
	}
	if (match != -1) {		/* option found */
		if (long_options[match].has_arg == vopt_long_no_argument
		    && has_equal) {
			/*
			 * XXX: GNU sets vopt_opt to val regardless of flag
			 */
			if (long_options[match].flag == NULL)
				vopt_opt = long_options[match].val;
			else
				vopt_opt = 0;
			return (VOPT_LONG_BADCH);
		}
		if (long_options[match].has_arg == vopt_long_required_argument ||
		    long_options[match].has_arg == vopt_long_optional_argument) {
			if (has_equal)
				vopt_arg = has_equal;
			else if (long_options[match].has_arg ==
			    vopt_long_required_argument) {
				/*
				 * optional argument doesn't use next nargv
				 */
				vopt_arg = nargv[vopt_ind++];
			}
		}
		if ((long_options[match].has_arg == vopt_long_required_argument)
		    && (vopt_arg == NULL)) {
			/*
			 * Missing argument; leading ':' indicates no error
			 * should be generated.
			 */
			/*
			 * XXX: GNU sets vopt_opt to val regardless of flag
			 */
			if (long_options[match].flag == NULL)
				vopt_opt = long_options[match].val;
			else
				vopt_opt = 0;
			--vopt_ind;
			return (VOPT_LONG_BADARG);
		}
	} else {			/* unknown option */
		if (short_too) {
			--vopt_ind;
			return (-1);
		}
		vopt_opt = 0;
		return (VOPT_LONG_BADCH);
	}
	if (idx)
		*idx = match;
	if (long_options[match].flag) {
		*long_options[match].flag = long_options[match].val;
		return (0);
	} else
		return (long_options[match].val);
}

/*
 * vopt_get_long_internal --
 *	Parse argc/argv argument vector.  Called by user level routines.
 */
static int
vopt_get_long_internal(int nargc, char **nargv, const char *options,
    const struct vopt_option *long_options, int *idx, int flags)
{
	char *oli;				/* option letter list index */
	int optchar, short_too;

	if (options == NULL)
		return (-1);

	/*
	 * Disable GNU extensions if POSIXLY_CORRECT is set or options
	 * string begins with a '+'.
	 */
	if (*options == '-')
		flags |= VOPT_LONG_FLAG_ALLARGS;
	else if (*options == '+')
		flags &= ~VOPT_LONG_FLAG_PERMUTE;
	if (*options == '+' || *options == '-')
		options++;

	/*
	 * XXX Some GNU programs (like cvs) set vopt_ind to 0 instead of
	 * XXX using vopt_reset.  Work around this braindamage.
	 */
	if (vopt_ind == 0)
		vopt_ind = vopt_reset = 1;

	vopt_arg = NULL;
	if (vopt_reset)
		vopt_long_nonopt_start = vopt_long_nonopt_end = -1;
start:
	/* update scanning pointer */
	if (vopt_reset || !*vopt_long_place) {
		vopt_reset = 0;
		if (vopt_ind >= nargc) {          /* end of argument vector */
			vopt_long_place = VOPT_LONG_EMSG;
			if (vopt_long_nonopt_end != -1) {
				/* do permutation, if we have to */
				vopt_long_permute_args(vopt_long_nonopt_start,
				    vopt_long_nonopt_end, vopt_ind, nargv);
				vopt_ind -= vopt_long_nonopt_end -
				    vopt_long_nonopt_start;
			}
			else if (vopt_long_nonopt_start != -1) {
				/*
				 * If we skipped non-options, set vopt_ind
				 * to the first of them.
				 */
				vopt_ind = vopt_long_nonopt_start;
			}
			vopt_long_nonopt_start = vopt_long_nonopt_end = -1;
			return (-1);
		}
		if (*(vopt_long_place = nargv[vopt_ind]) != '-' ||
		    vopt_long_place[1] == '\0') {
			/* found non-option */
			vopt_long_place = VOPT_LONG_EMSG;
			if (flags & VOPT_LONG_FLAG_ALLARGS) {
				/*
				 * GNU extension:
				 * return non-option as argument to option 1
				 */
				vopt_arg = nargv[vopt_ind++];
				return (VOPT_LONG_INORDER);
			}
			if (!(flags & VOPT_LONG_FLAG_PERMUTE)) {
				/*
				 * If no permutation wanted, stop parsing
				 * at first non-option.
				 */
				return (-1);
			}
			/* do permutation */
			if (vopt_long_nonopt_start == -1)
				vopt_long_nonopt_start = vopt_ind;
			else if (vopt_long_nonopt_end != -1) {
				vopt_long_permute_args(vopt_long_nonopt_start,
				    vopt_long_nonopt_end,
				    vopt_ind, nargv);
				vopt_long_nonopt_start = vopt_ind -
				    (vopt_long_nonopt_end -
				     vopt_long_nonopt_start);
				vopt_long_nonopt_end = -1;
			}
			vopt_ind++;
			/* process next argument */
			goto start;
		}
		if (vopt_long_nonopt_start != -1 && vopt_long_nonopt_end == -1)
			vopt_long_nonopt_end = vopt_ind;

		/*
		 * If we have "-" do nothing, if "--" we are done.
		 */
		if (vopt_long_place[1] != '\0' && *++vopt_long_place == '-' &&
		    vopt_long_place[1] == '\0') {
			vopt_ind++;
			vopt_long_place = VOPT_LONG_EMSG;
			/*
			 * We found an option (--), so if we skipped
			 * non-options, we have to permute.
			 */
			if (vopt_long_nonopt_end != -1) {
				vopt_long_permute_args(vopt_long_nonopt_start,
				    vopt_long_nonopt_end,
				    vopt_ind, nargv);
				vopt_ind -= vopt_long_nonopt_end -
				    vopt_long_nonopt_start;
			}
			vopt_long_nonopt_start = vopt_long_nonopt_end = -1;
			return (-1);
		}
	}

	/*
	 * Check long options if:
	 *  1) we were passed some
	 *  2) the arg is not just "-"
	 *  3) either the arg starts with -- we are VOPT_long_only()
	 */
	if (long_options != NULL && vopt_long_place != nargv[vopt_ind] &&
	    (*vopt_long_place == '-' || (flags & VOPT_LONG_FLAG_LONGONLY))) {
		short_too = 0;
		if (*vopt_long_place == '-') {
			vopt_long_place++;		/* --foo long option */
		} else if (*vopt_long_place != ':' &&
		    strchr(options, *vopt_long_place) != NULL)
			short_too = 1;		/* could be short option too */

		optchar = vopt_long_parse_options(nargv, options, long_options,
		    idx, short_too, flags);
		if (optchar != -1) {
			vopt_long_place = VOPT_LONG_EMSG;
			return (optchar);
		}
	}

	if ((optchar = (int)*vopt_long_place++) == (int)':' ||
	    (optchar == (int)'-' && *vopt_long_place != '\0') ||
	    (oli = strchr(options, optchar)) == NULL) {
		/*
		 * If the user specified "-" and  '-' isn't listed in
		 * options, return -1 (non-option) as per POSIX.
		 * Otherwise, it is an unknown option character (or ':').
		 */
		if (optchar == (int)'-' && *vopt_long_place == '\0')
			return (-1);
		if (!*vopt_long_place)
			++vopt_ind;
		vopt_opt = optchar;
		return (VOPT_LONG_BADCH);
	}
	if (long_options != NULL && optchar == 'W' && oli[1] == ';') {
		/* -W long-option */
		if (*vopt_long_place)			/* no space */
			/* NOTHING */;
		else if (++vopt_ind >= nargc) {	/* no arg */
			vopt_long_place = VOPT_LONG_EMSG;
			vopt_opt = optchar;
			return (VOPT_LONG_BADARG);
		} else				/* white space */
			vopt_long_place = nargv[vopt_ind];
		optchar = vopt_long_parse_options(nargv, options, long_options,
		    idx, 0, flags);
		vopt_long_place = VOPT_LONG_EMSG;
		return (optchar);
	}
	if (*++oli != ':') {			/* doesn't take argument */
		if (!*vopt_long_place)
			++vopt_ind;
	} else {				/* takes (optional) argument */
		vopt_arg = NULL;
		if (*vopt_long_place)			/* no white space */
			vopt_arg = vopt_long_place;
		else if (oli[1] != ':') {	/* arg not optional */
			if (++vopt_ind >= nargc) {	/* no arg */
				vopt_long_place = VOPT_LONG_EMSG;
				vopt_opt = optchar;
				return (VOPT_LONG_BADARG);
			} else
				vopt_arg = nargv[vopt_ind];
		}
		vopt_long_place = VOPT_LONG_EMSG;
		++vopt_ind;
	}
	/* dump back option letter */
	return (optchar);
}

/*
 * VOPT_long --
 *	Parse argc/argv argument vector.
 */
int
VOPT_get_long(int nargc, char **nargv, const char *options,
    const struct vopt_option *long_options, int *idx)
{

	return (vopt_get_long_internal(nargc, nargv, options, long_options, idx,
		VOPT_LONG_FLAG_PERMUTE));
}
