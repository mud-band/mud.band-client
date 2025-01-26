#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "crypto.h"

void
crypto_zero(void *dest, size_t len)
{
	volatile uint8_t *p = (uint8_t *)dest;

	while (len--) {
		*p++ = 0;
	}
}

bool
crypto_equal(const uint8_t *a, const uint8_t *b, size_t size)
{
	uint8_t neq = 0;

	while (size > 0) {
		neq |= *(const uint8_t *)a ^ *(const uint8_t *)b;
		a += 1;
		b += 1;
		size -= 1;
	}
	return (neq) ? false : true;
}
