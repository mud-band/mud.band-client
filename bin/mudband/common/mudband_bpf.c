#if defined(WIN32)
#include <windows.h>
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mudband_bpf.h"

/*
 * Number of scratch memory words (for BPF_LD|BPF_MEM and BPF_ST).
 */
#define BPF_MEMWORDS 16

#ifndef BPF_ALIGN
#define EXTRACT_SHORT(p)	((uint16_t)ntohs(*(uint16_t *)p))
#define EXTRACT_LONG(p)		(ntohl(*(uint32_t *)p))
#else
#define EXTRACT_SHORT(p)\
	((uint16_t)\
		((uint16_t)*((uint8_t *)p+0)<<8|\
		 (uint16_t)*((uint8_t *)p+1)<<0))
#define EXTRACT_LONG(p)\
		((uint32_t)*((uint8_t *)p+0)<<24|\
		 (uint32_t)*((uint8_t *)p+1)<<16|\
		 (uint32_t)*((uint8_t *)p+2)<<8|\
		 (uint32_t)*((uint8_t *)p+3)<<0)
#endif

/*
 * The instruction encodings.
 *
 * Please inform tcpdump-workers@lists.tcpdump.org if you use any
 * of the reserved values, so that we can note that they're used
 * (and perhaps implement it in the reference BPF implementation
 * and encourage its implementation elsewhere).
 */

/*
 * The upper 8 bits of the opcode aren't used. BSD/OS used 0x8000.
 */

/* instruction classes */
#define BPF_CLASS(code) ((code) & 0x07)
#define		BPF_LD		0x00
#define		BPF_LDX		0x01
#define		BPF_ST		0x02
#define		BPF_STX		0x03
#define		BPF_ALU		0x04
#define		BPF_JMP		0x05
#define		BPF_RET		0x06
#define		BPF_MISC	0x07

/* ld/ldx fields */
#define BPF_SIZE(code)	((code) & 0x18)
#define		BPF_W		0x00
#define		BPF_H		0x08
#define		BPF_B		0x10
/*				0x18	reserved; used by BSD/OS */
#define BPF_MODE(code)	((code) & 0xe0)
#define		BPF_IMM	0x00
#define		BPF_ABS		0x20
#define		BPF_IND		0x40
#define		BPF_MEM		0x60
#define		BPF_LEN		0x80
#define		BPF_MSH		0xa0
/*				0xc0	reserved; used by BSD/OS */
/*				0xe0	reserved; used by BSD/OS */

/* alu/jmp fields */
#define BPF_OP(code)	((code) & 0xf0)
#define		BPF_ADD		0x00
#define		BPF_SUB		0x10
#define		BPF_MUL		0x20
#define		BPF_DIV		0x30
#define		BPF_OR		0x40
#define		BPF_AND		0x50
#define		BPF_LSH		0x60
#define		BPF_RSH		0x70
#define		BPF_NEG		0x80
#define		BPF_MOD		0x90
#define		BPF_XOR		0xa0
/*				0xb0	reserved */
/*				0xc0	reserved */
/*				0xd0	reserved */
/*				0xe0	reserved */
/*				0xf0	reserved */

#define		BPF_JA		0x00
#define		BPF_JEQ		0x10
#define		BPF_JGT		0x20
#define		BPF_JGE		0x30
#define		BPF_JSET	0x40
/*				0x50	reserved; used on BSD/OS */
/*				0x60	reserved */
/*				0x70	reserved */
/*				0x80	reserved */
/*				0x90	reserved */
/*				0xa0	reserved */
/*				0xb0	reserved */
/*				0xc0	reserved */
/*				0xd0	reserved */
/*				0xe0	reserved */
/*				0xf0	reserved */
#define BPF_SRC(code)	((code) & 0x08)
#define		BPF_K		0x00
#define		BPF_X		0x08

/* ret - BPF_K and BPF_X also apply */
#define BPF_RVAL(code)	((code) & 0x18)
#define		BPF_A		0x10
/*				0x18	reserved */

/* misc */
#define BPF_MISCOP(code) ((code) & 0xf8)
#define		BPF_TAX		0x00
/*				0x08	reserved */
/*				0x10	reserved */
/*				0x18	reserved */
/* #define	BPF_COP		0x20	NetBSD "coprocessor" extensions */
/*				0x28	reserved */
/*				0x30	reserved */
/*				0x38	reserved */
/* #define	BPF_COPX	0x40	NetBSD "coprocessor" extensions */
/*					also used on BSD/OS */
/*				0x48	reserved */
/*				0x50	reserved */
/*				0x58	reserved */
/*				0x60	reserved */
/*				0x68	reserved */
/*				0x70	reserved */
/*				0x78	reserved */
#define		BPF_TXA		0x80
/*				0x88	reserved */
/*				0x90	reserved */
/*				0x98	reserved */
/*				0xa0	reserved */
/*				0xa8	reserved */
/*				0xb0	reserved */
/*				0xb8	reserved */
/*				0xc0	reserved; used on BSD/OS */
/*				0xc8	reserved */
/*				0xd0	reserved */
/*				0xd8	reserved */
/*				0xe0	reserved */
/*				0xe8	reserved */
/*				0xf0	reserved */
/*				0xf8	reserved */

/*
 * Execute the filter program starting at pc on the packet p
 * wirelen is the length of the original packet
 * buflen is the amount of data present
 */
uint32_t
mudband_bpf_filter(const struct mudband_bpf_insn *pc, uint8_t *p, uint32_t wirelen,
    uint32_t buflen)
{
	uint32_t A = 0, X = 0;
    mudband_bpf_u_int32 k;
	uint32_t mem[BPF_MEMWORDS];

	memset(mem, 0, sizeof(mem));

	if (pc == NULL)
		/*
		 * No filter means accept all.
		 */
		return ((uint32_t)-1);

	--pc;
	while (1) {
		++pc;
		switch (pc->code) {
		default:
			abort();
		case BPF_RET|BPF_K:
			return ((uint32_t)pc->k);

		case BPF_RET|BPF_A:
			return ((uint32_t)A);

		case BPF_LD|BPF_W|BPF_ABS:
			k = pc->k;
			if (k > buflen || sizeof(int32_t) > buflen - k) {
				return (0);
			}
#ifdef BPF_ALIGN
			if (((intptr_t)(p + k) & 3) != 0)
				A = EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(int32_t *)(p + k));
			continue;

		case BPF_LD|BPF_H|BPF_ABS:
			k = pc->k;
			if (k > buflen || sizeof(int16_t) > buflen - k) {
				return (0);
			}
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_ABS:
			k = pc->k;
			if (k >= buflen) {
				return (0);
			}
			A = p[k];
			continue;

		case BPF_LD|BPF_W|BPF_LEN:
			A = wirelen;
			continue;

		case BPF_LDX|BPF_W|BPF_LEN:
			X = wirelen;
			continue;

		case BPF_LD|BPF_W|BPF_IND:
			k = X + pc->k;
			if (pc->k > buflen || X > buflen - pc->k ||
			    sizeof(int32_t) > buflen - k) {
				return (0);
			}
#ifdef BPF_ALIGN
			if (((intptr_t)(p + k) & 3) != 0)
				A = EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(int32_t *)(p + k));
			continue;

		case BPF_LD|BPF_H|BPF_IND:
			k = X + pc->k;
			if (X > buflen || pc->k > buflen - X ||
			    sizeof(int16_t) > buflen - k) {
				return (0);
			}
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_IND:
			k = X + pc->k;
			if (pc->k >= buflen || X >= buflen - pc->k) {
				return (0);
			}
			A = p[k];
			continue;

		case BPF_LDX|BPF_MSH|BPF_B:
			k = pc->k;
			if (k >= buflen) {
				return (0);
			}
			X = (p[pc->k] & 0xf) << 2;
			continue;

		case BPF_LD|BPF_IMM:
			A = pc->k;
			continue;

		case BPF_LDX|BPF_IMM:
			X = pc->k;
			continue;

		case BPF_LD|BPF_MEM:
			A = mem[pc->k];
			continue;

		case BPF_LDX|BPF_MEM:
			X = mem[pc->k];
			continue;

		case BPF_ST:
			mem[pc->k] = A;
			continue;

		case BPF_STX:
			mem[pc->k] = X;
			continue;

		case BPF_JMP|BPF_JA:
			pc += pc->k;
			continue;

		case BPF_JMP|BPF_JGT|BPF_K:
			pc += (A > pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_K:
			pc += (A >= pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_K:
			pc += (A == pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_K:
			pc += (A & pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGT|BPF_X:
			pc += (A > X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_X:
			pc += (A >= X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_X:
			pc += (A == X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_X:
			pc += (A & X) ? pc->jt : pc->jf;
			continue;

		case BPF_ALU|BPF_ADD|BPF_X:
			A += X;
			continue;

		case BPF_ALU|BPF_SUB|BPF_X:
			A -= X;
			continue;

		case BPF_ALU|BPF_MUL|BPF_X:
			A *= X;
			continue;

		case BPF_ALU|BPF_DIV|BPF_X:
			if (X == 0)
				return (0);
			A /= X;
			continue;

		case BPF_ALU|BPF_AND|BPF_X:
			A &= X;
			continue;

		case BPF_ALU|BPF_OR|BPF_X:
			A |= X;
			continue;

		case BPF_ALU|BPF_LSH|BPF_X:
			A <<= X;
			continue;

		case BPF_ALU|BPF_RSH|BPF_X:
			A >>= X;
			continue;

		case BPF_ALU|BPF_ADD|BPF_K:
			A += pc->k;
			continue;

		case BPF_ALU|BPF_SUB|BPF_K:
			A -= pc->k;
			continue;

		case BPF_ALU|BPF_MUL|BPF_K:
			A *= pc->k;
			continue;

		case BPF_ALU|BPF_DIV|BPF_K:
			A /= pc->k;
			continue;

		case BPF_ALU|BPF_AND|BPF_K:
			A &= pc->k;
			continue;

		case BPF_ALU|BPF_OR|BPF_K:
			A |= pc->k;
			continue;

		case BPF_ALU|BPF_LSH|BPF_K:
			A <<= pc->k;
			continue;

		case BPF_ALU|BPF_RSH|BPF_K:
			A >>= pc->k;
			continue;

		case BPF_ALU|BPF_NEG:
			A = -A;
			continue;

		case BPF_MISC|BPF_TAX:
			X = A;
			continue;

		case BPF_MISC|BPF_TXA:
			A = X;
			continue;
		}
	}
}

static const uint16_t	bpf_code_map[] = {
	0x10ff,	/* 0x00-0x0f: 1111111100001000 */
	0x3070,	/* 0x10-0x1f: 0000111000001100 */
	0x3131,	/* 0x20-0x2f: 1000110010001100 */
	0x3031,	/* 0x30-0x3f: 1000110000001100 */
	0x3131,	/* 0x40-0x4f: 1000110010001100 */
	0x1011,	/* 0x50-0x5f: 1000100000001000 */
	0x1013,	/* 0x60-0x6f: 1100100000001000 */
	0x1010,	/* 0x70-0x7f: 0000100000001000 */
	0x0093,	/* 0x80-0x8f: 1100100100000000 */
	0x0000,	/* 0x90-0x9f: 0000000000000000 */
	0x0000,	/* 0xa0-0xaf: 0000000000000000 */
	0x0002,	/* 0xb0-0xbf: 0100000000000000 */
	0x0000,	/* 0xc0-0xcf: 0000000000000000 */
	0x0000,	/* 0xd0-0xdf: 0000000000000000 */
	0x0000,	/* 0xe0-0xef: 0000000000000000 */
	0x0000	/* 0xf0-0xff: 0000000000000000 */
};

#define	BPF_VALIDATE_CODE(c)	\
    ((c) <= 0xff && (bpf_code_map[(c) >> 4] & (1 << ((c) & 0xf))) != 0)

/*
 * Return true if the 'fcode' is a valid filter program.
 * The constraints are that each jump be forward and to a valid
 * code.  The code must terminate with either an accept or reject.
 *
 * The kernel needs to be able to verify an application's filter code.
 * Otherwise, a bogus program could easily crash the system.
 */
int
mudband_bpf_validate(const struct mudband_bpf_insn *f, int len)
{
	register int i;
	register const struct mudband_bpf_insn *p;

	/* Do not accept negative length filter. */
	if (len < 0)
		return (0);

	/* An empty filter means accept all. */
	if (len == 0)
		return (1);

	for (i = 0; i < len; ++i) {
		p = &f[i];
		/*
		 * Check that the code is valid.
		 */
		if (!BPF_VALIDATE_CODE(p->code))
			return (0);
		/*
		 * Check that that jumps are forward, and within
		 * the code block.
		 */
		if (BPF_CLASS(p->code) == BPF_JMP) {
			register uint32_t offset;

			if (p->code == (BPF_JMP|BPF_JA))
				offset = p->k;
			else
				offset = p->jt > p->jf ? p->jt : p->jf;
			if (offset >= (uint32_t)(len - i) - 1)
				return (0);
			continue;
		}
		/*
		 * Check that memory operations use valid addresses.
		 */
		if (p->code == BPF_ST || p->code == BPF_STX ||
		    p->code == (BPF_LD|BPF_MEM) ||
		    p->code == (BPF_LDX|BPF_MEM)) {
			if (p->k >= BPF_MEMWORDS)
				return (0);
			continue;
		}
		/*
		 * Check for constant division by 0.
		 */
		if (p->code == (BPF_ALU|BPF_DIV|BPF_K) && p->k == 0)
			return (0);
	}
	return (BPF_CLASS(f[len - 1].code) == BPF_RET);
}
