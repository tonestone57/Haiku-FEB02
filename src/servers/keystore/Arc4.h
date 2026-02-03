/*
 * Copyright (c) 2003 Markus Friedl <markus@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ARC4_H
#define _ARC4_H

#include <SupportDefs.h>

#define RC4STATE 256
#define RC4KEYLEN 16

struct rc4_ctx {
	uint8 x, y;
	uint8 state[RC4STATE];
};

void	rc4_keysetup(struct rc4_ctx *, uint8 *, uint32);
void	rc4_crypt(struct rc4_ctx *, uint8 *, uint8 *, uint32);
void	rc4_getbytes(struct rc4_ctx *, uint8 *, uint32);
void	rc4_skip(struct rc4_ctx *, uint32);

#endif // _ARC4_H
