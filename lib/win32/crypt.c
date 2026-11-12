// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * crypt() for Windows
 * XXX: this file is LLM generated, should we a library
 * for Windows, let's use it to avoid the burden of the maintenance.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char *crypt(const char *key, const char *salt);

/* Longest result: "$6$rounds=999999999$", 16 of salt, '$', 86 of hash, NUL */
#define CRYPT_OUT_LEN 128

/* The 64 digits of every scheme, in increasing value */
static const char crypt_b64[] =
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define MD_BLOCK_MAX  128
#define MD_DIGEST_MAX 64

struct md_algo {
	size_t block_len;
	size_t digest_len;
	size_t word_len; /* of the state; the length field is two words */
	bool big_endian;
	uint64_t iv[8];
	void (*compress)(uint64_t *state, const uint8_t *block);
	/* order in which crypt() encodes the bytes of the digest */
	uint8_t crypt_order[MD_DIGEST_MAX];
};

struct md_ctx {
	const struct md_algo *algo;
	uint64_t state[8]; /* MD5 and SHA-256 only use the low 32 bits */
	uint64_t len;	   /* bytes hashed so far */
	uint8_t buf[MD_BLOCK_MAX];
};

static inline uint32_t rol32(uint32_t v, unsigned int n)
{
	return (v << n) | (v >> (32 - n));
}

static inline uint32_t ror32(uint32_t v, unsigned int n)
{
	return (v >> n) | (v << (32 - n));
}

static inline uint64_t ror64(uint64_t v, unsigned int n)
{
	return (v >> n) | (v << (64 - n));
}

static uint64_t load_word(const uint8_t *p, size_t n, bool big_endian)
{
	uint64_t v = 0;
	size_t i;

	for (i = 0; i < n; i++)
		v = (v << 8) | p[big_endian ? i : n - 1 - i];
	return v;
}

static void store_word(uint8_t *p, uint64_t v, size_t n, bool big_endian)
{
	size_t i;

	for (i = 0; i < n; i++) {
		p[big_endian ? n - 1 - i : i] = (uint8_t)v;
		v >>= 8;
	}
}

/* clang-format off */

/* MD5: floor(2^32 * abs(sin(i + 1))) */
static const uint32_t md5_k[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
	0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
	0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
	0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
	0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
	0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

/* SHA-256: fractional part of the cube root of the first 64 primes */
static const uint32_t sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

/* SHA-512: the same for 80 primes, with 64 bits */
static const uint64_t sha512_k[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
	0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
	0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
	0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
	0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
	0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
	0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
	0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
	0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
	0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
	0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
	0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
	0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
	0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

/* clang-format on */

static void md5_compress(uint64_t *state, const uint8_t *block)
{
	static const uint8_t shift[4][4] = {
		{ 7, 12, 17, 22 },
		{ 5, 9, 14, 20 },
		{ 4, 11, 16, 23 },
		{ 6, 10, 15, 21 },
	};
	uint32_t a = (uint32_t)state[0], b = (uint32_t)state[1];
	uint32_t c = (uint32_t)state[2], d = (uint32_t)state[3];
	uint32_t x[16], f;
	unsigned int i, g;

	for (i = 0; i < 16; i++)
		x[i] = (uint32_t)load_word(block + 4 * i, 4, false);

	for (i = 0; i < 64; i++) {
		switch (i / 16) {
		case 0:
			f = (b & c) | (~b & d);
			g = i;
			break;
		case 1:
			f = (d & b) | (~d & c);
			g = (5 * i + 1) % 16;
			break;
		case 2:
			f = b ^ c ^ d;
			g = (3 * i + 5) % 16;
			break;
		default:
			f = c ^ (b | ~d);
			g = (7 * i) % 16;
			break;
		}
		f += a + md5_k[i] + x[g];
		a = d;
		d = c;
		c = b;
		b += rol32(f, shift[i / 16][i % 4]);
	}

	state[0] = (uint32_t)(state[0] + a);
	state[1] = (uint32_t)(state[1] + b);
	state[2] = (uint32_t)(state[2] + c);
	state[3] = (uint32_t)(state[3] + d);
}

static void sha256_compress(uint64_t *state, const uint8_t *block)
{
	uint32_t w[64], v[8], t1, t2;
	unsigned int i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)load_word(block + 4 * i, 4, true);
	for (; i < 64; i++) {
		t1 = w[i - 15];
		t2 = w[i - 2];
		w[i] = w[i - 16] + w[i - 7] +
		       (ror32(t1, 7) ^ ror32(t1, 18) ^ (t1 >> 3)) +
		       (ror32(t2, 17) ^ ror32(t2, 19) ^ (t2 >> 10));
	}

	for (i = 0; i < 8; i++)
		v[i] = (uint32_t)state[i];

	for (i = 0; i < 64; i++) {
		t1 = v[7] + sha256_k[i] + w[i] +
		     (ror32(v[4], 6) ^ ror32(v[4], 11) ^ ror32(v[4], 25)) +
		     ((v[4] & v[5]) ^ (~v[4] & v[6]));
		t2 = (ror32(v[0], 2) ^ ror32(v[0], 13) ^ ror32(v[0], 22)) +
		     ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
		/* every working variable moves down one place */
		memmove(v + 1, v, 7 * sizeof(v[0]));
		v[4] += t1;
		v[0] = t1 + t2;
	}

	for (i = 0; i < 8; i++)
		state[i] = (uint32_t)(state[i] + v[i]);
}

static void sha512_compress(uint64_t *state, const uint8_t *block)
{
	uint64_t w[80], v[8], t1, t2;
	unsigned int i;

	for (i = 0; i < 16; i++)
		w[i] = load_word(block + 8 * i, 8, true);
	for (; i < 80; i++) {
		t1 = w[i - 15];
		t2 = w[i - 2];
		w[i] = w[i - 16] + w[i - 7] +
		       (ror64(t1, 1) ^ ror64(t1, 8) ^ (t1 >> 7)) +
		       (ror64(t2, 19) ^ ror64(t2, 61) ^ (t2 >> 6));
	}

	memcpy(v, state, sizeof(v));

	for (i = 0; i < 80; i++) {
		t1 = v[7] + sha512_k[i] + w[i] +
		     (ror64(v[4], 14) ^ ror64(v[4], 18) ^ ror64(v[4], 41)) +
		     ((v[4] & v[5]) ^ (~v[4] & v[6]));
		t2 = (ror64(v[0], 28) ^ ror64(v[0], 34) ^ ror64(v[0], 39)) +
		     ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
		/* every working variable moves down one place */
		memmove(v + 1, v, 7 * sizeof(v[0]));
		v[4] += t1;
		v[0] = t1 + t2;
	}

	for (i = 0; i < 8; i++)
		state[i] += v[i];
}

/* clang-format off */

static const struct md_algo md5 = {
	.block_len = 64,
	.digest_len = 16,
	.word_len = 4,
	.big_endian = false,
	.iv = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 },
	.compress = md5_compress,
	.crypt_order = {
		12, 6, 0, 13, 7, 1, 14, 8, 2, 15, 9, 3, 5, 10, 4, 11,
	},
};

/* SHA-2 starts from the fractional part of the square root of 8 primes. */
static const struct md_algo sha256 = {
	.block_len = 64,
	.digest_len = 32,
	.word_len = 4,
	.big_endian = true,
	.iv = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	},
	.compress = sha256_compress,
	.crypt_order = {
		20, 10,  0, 11,  1, 21,  2, 22, 12, 23, 13,  3, 14,  4, 24,
		 5, 25, 15, 26, 16,  6, 17,  7, 27,  8, 28, 18, 29, 19,  9,
		30, 31,
	},
};

static const struct md_algo sha512 = {
	.block_len = 128,
	.digest_len = 64,
	.word_len = 8,
	.big_endian = true,
	.iv = {
		0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
		0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
		0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
		0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
	},
	.compress = sha512_compress,
	.crypt_order = {
		42, 21,  0,  1, 43, 22, 23,  2, 44, 45, 24,  3,  4, 46, 25,
		26,  5, 47, 48, 27,  6,  7, 49, 28, 29,  8, 50, 51, 30,  9,
		10, 52, 31, 32, 11, 53, 54, 33, 12, 13, 55, 34, 35, 14, 56,
		57, 36, 15, 16, 58, 37, 38, 17, 59, 60, 39, 18, 19, 61, 40,
		41, 20, 62, 63,
	},
};

/* clang-format on */

static void md_init(struct md_ctx *ctx, const struct md_algo *algo)
{
	ctx->algo = algo;
	memcpy(ctx->state, algo->iv, sizeof(ctx->state));
	ctx->len = 0;
}

static void md_update(struct md_ctx *ctx, const void *data, size_t len)
{
	size_t block_len = ctx->algo->block_len;
	size_t used = (size_t)(ctx->len % block_len), n;
	const uint8_t *p = data;

	ctx->len += len;
	while (len) {
		n = block_len - used < len ? block_len - used : len;
		memcpy(ctx->buf + used, p, n);
		used += n;
		p += n;
		len -= n;
		if (used == block_len) {
			ctx->algo->compress(ctx->state, ctx->buf);
			used = 0;
		}
	}
}

/* Hash len bytes made of the digest repeated as often as it takes. */
static void md_update_repeat(struct md_ctx *ctx, const uint8_t *digest,
			     size_t len)
{
	size_t digest_len = ctx->algo->digest_len;

	for (; len > digest_len; len -= digest_len)
		md_update(ctx, digest, digest_len);
	md_update(ctx, digest, len);
}

static void md_final(struct md_ctx *ctx, uint8_t *digest)
{
	const struct md_algo *algo = ctx->algo;
	uint8_t pad[MD_BLOCK_MAX + 16] = { 0x80 };
	size_t len_len = 2 * algo->word_len;
	size_t used = (size_t)(ctx->len % algo->block_len);
	/* at least the 0x80 byte, then up to where the length ends a block */
	size_t pad_len = algo->block_len - (used + len_len) % algo->block_len;
	size_t i;

	/* The length in bits: 64 bits are plenty, even in a 128 bit field. */
	store_word(pad + pad_len + len_len - 8, ctx->len << 3, 8,
		   algo->big_endian);
	md_update(ctx, pad, pad_len + len_len);

	for (i = 0; i < algo->digest_len / algo->word_len; i++)
		store_word(digest + i * algo->word_len, ctx->state[i],
			   algo->word_len, algo->big_endian);
}

static void md_encode(const struct md_algo *algo, const uint8_t *digest,
		      char *out)
{
	const uint8_t *order = algo->crypt_order;
	size_t i, j, n;
	uint32_t v;

	for (i = 0; i < algo->digest_len; i += n) {
		n = algo->digest_len - i < 3 ? algo->digest_len - i : 3;
		v = 0;
		for (j = 0; j < n; j++)
			v |= (uint32_t)digest[order[i + j]] << (8 * j);
		/* n bytes need n + 1 digits */
		for (j = 0; j <= n; j++) {
			*out++ = crypt_b64[v & 0x3f];
			v >>= 6;
		}
	}
	*out = '\0';
}

static char *put_bytes(char *out, const char *s, size_t len)
{
	memcpy(out, s, len);
	return out + len;
}

static char *put_decimal(char *out, uint32_t v)
{
	char digits[10];
	size_t n = 0;

	do {
		digits[n++] = (char)('0' + v % 10);
		v /= 10;
	} while (v);
	while (n)
		*out++ = digits[--n];
	return out;
}

#define MD5_SALT_MAX 8
#define MD5_ROUNDS   1000

static void md5_crypt(const char *key, const char *setting, char *out)
{
	const char *salt = setting + 3;
	size_t key_len = strlen(key), salt_len = strcspn(salt, "$"), n;
	uint8_t digest[MD_DIGEST_MAX];
	struct md_ctx ctx;
	unsigned int i;

	if (salt_len > MD5_SALT_MAX)
		salt_len = MD5_SALT_MAX;

	md_init(&ctx, &md5);
	md_update(&ctx, key, key_len);
	md_update(&ctx, salt, salt_len);
	md_update(&ctx, key, key_len);
	md_final(&ctx, digest);

	md_init(&ctx, &md5);
	md_update(&ctx, key, key_len);
	md_update(&ctx, setting, 3);
	md_update(&ctx, salt, salt_len);
	md_update_repeat(&ctx, digest, key_len);
	/* An oddity of the scheme: a NUL or the first byte of the key. */
	for (n = key_len; n; n >>= 1)
		md_update(&ctx, (n & 1) ? "" : key, 1);
	md_final(&ctx, digest);

	/* What makes the hash slow; the round decides what gets hashed. */
	for (i = 0; i < MD5_ROUNDS; i++) {
		md_init(&ctx, &md5);
		if (i & 1)
			md_update(&ctx, key, key_len);
		else
			md_update(&ctx, digest, md5.digest_len);
		if (i % 3)
			md_update(&ctx, salt, salt_len);
		if (i % 7)
			md_update(&ctx, key, key_len);
		if (i & 1)
			md_update(&ctx, digest, md5.digest_len);
		else
			md_update(&ctx, key, key_len);
		md_final(&ctx, digest);
	}

	out = put_bytes(out, setting, 3);
	out = put_bytes(out, salt, salt_len);
	*out++ = '$';
	md_encode(&md5, digest, out);
}

#define SHA_SALT_MAX	   16
#define SHA_ROUNDS_DEFAULT 5000
#define SHA_ROUNDS_MIN	   1000
#define SHA_ROUNDS_MAX	   999999999

static void sha_crypt(const struct md_algo *algo, const char *key,
		      const char *setting, char *out)
{
	uint8_t digest[MD_DIGEST_MAX], dp[MD_DIGEST_MAX], ds[MD_DIGEST_MAX];
	const char *salt = setting + 3;
	size_t key_len = strlen(key), salt_len, n;
	uint32_t rounds = SHA_ROUNDS_DEFAULT, i;
	bool rounds_given = false;
	unsigned long num;
	struct md_ctx ctx;
	char *end;

	if (!strncmp(salt, "rounds=", 7)) {
		/*
		 * strtoul() saturates, so the clamping is right whatever
		 * the width of long.  Without the '$', this is all salt.
		 */
		num = strtoul(salt + 7, &end, 10);
		if (*end == '$') {
			if (num < SHA_ROUNDS_MIN)
				num = SHA_ROUNDS_MIN;
			else if (num > SHA_ROUNDS_MAX)
				num = SHA_ROUNDS_MAX;
			rounds = (uint32_t)num;
			rounds_given = true;
			salt = end + 1;
		}
	}
	salt_len = strcspn(salt, "$");
	if (salt_len > SHA_SALT_MAX)
		salt_len = SHA_SALT_MAX;

	/* Digest B: key, salt, key. */
	md_init(&ctx, algo);
	md_update(&ctx, key, key_len);
	md_update(&ctx, salt, salt_len);
	md_update(&ctx, key, key_len);
	md_final(&ctx, digest);

	/*
	 * Digest A: key, salt, B stretched to the length of the key, then
	 * B or the key for each bit of that length.
	 */
	md_init(&ctx, algo);
	md_update(&ctx, key, key_len);
	md_update(&ctx, salt, salt_len);
	md_update_repeat(&ctx, digest, key_len);
	for (n = key_len; n; n >>= 1) {
		if (n & 1)
			md_update(&ctx, digest, algo->digest_len);
		else
			md_update(&ctx, key, key_len);
	}
	md_final(&ctx, digest);

	/* Digest DP: the key, once for each of its bytes. */
	md_init(&ctx, algo);
	for (n = 0; n < key_len; n++)
		md_update(&ctx, key, key_len);
	md_final(&ctx, dp);

	/* Digest DS: the salt, 16 + A[0] times. */
	md_init(&ctx, algo);
	for (n = 0; n < 16 + (size_t)digest[0]; n++)
		md_update(&ctx, salt, salt_len);
	md_final(&ctx, ds);

	/*
	 * What makes the hash slow.  Each round hashes the last digest with
	 * P, which is DP stretched to the length of the key, and with S,
	 * the start of DS as long as the salt; the round decides which and
	 * in what order.
	 */
	for (i = 0; i < rounds; i++) {
		md_init(&ctx, algo);
		if (i & 1)
			md_update_repeat(&ctx, dp, key_len);
		else
			md_update(&ctx, digest, algo->digest_len);
		if (i % 3)
			md_update(&ctx, ds, salt_len);
		if (i % 7)
			md_update_repeat(&ctx, dp, key_len);
		if (i & 1)
			md_update(&ctx, digest, algo->digest_len);
		else
			md_update_repeat(&ctx, dp, key_len);
		md_final(&ctx, digest);
	}

	out = put_bytes(out, setting, 3);
	if (rounds_given) {
		out = put_bytes(out, "rounds=", 7);
		out = put_decimal(out, rounds);
		*out++ = '$';
	}
	out = put_bytes(out, salt, salt_len);
	*out++ = '$';
	md_encode(algo, digest, out);
}

/* clang-format off */

static const uint8_t des_pc1[56] = {
	57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,
	10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,
	63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,
	14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4,
};

static const uint8_t des_pc2[48] = {
	14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32,
};

static const uint8_t des_shifts[16] = {
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1,
};

static const uint8_t des_e[48] = {
	32,  1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,
	 8,  9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
	16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25,
	24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32,  1,
};

static const uint8_t des_sbox[8][64] = {
	{ 14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
	   0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
	   4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
	  15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13 },
	{ 15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
	   3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
	   0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
	  13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9 },
	{ 10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
	  13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
	  13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
	   1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12 },
	{  7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
	  13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
	  10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
	   3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14 },
	{  2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
	  14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
	   4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
	  11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3 },
	{ 12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
	  10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
	   9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
	   4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13 },
	{  4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
	  13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
	   1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
	   6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12 },
	{ 13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
	   1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
	   7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
	   2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11 },
};

static const uint8_t des_p[32] = {
	16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,
	 2,  8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25,
};

static const uint8_t des_fp[64] = {
	40,  8, 48, 16, 56, 24, 64, 32, 39,  7, 47, 15, 55, 23, 63, 31,
	38,  6, 46, 14, 54, 22, 62, 30, 37,  5, 45, 13, 53, 21, 61, 29,
	36,  4, 44, 12, 52, 20, 60, 28, 35,  3, 43, 11, 51, 19, 59, 27,
	34,  2, 42, 10, 50, 18, 58, 26, 33,  1, 41,  9, 49, 17, 57, 25,
};

/* clang-format on */

/* Bit i of the n_out bit result is bit tab[i] of the n_in bit input. */
static uint64_t des_permute(uint64_t in, const uint8_t *tab,
			    unsigned int n_out, unsigned int n_in)
{
	uint64_t out = 0;
	unsigned int i;

	for (i = 0; i < n_out; i++)
		out = (out << 1) | ((in >> (n_in - tab[i])) & 1);
	return out;
}

static inline uint32_t rol28(uint32_t v, unsigned int n)
{
	return ((v << n) | (v >> (28 - n))) & 0xfffffff;
}

/* The round function of DES, with the salt applied to the E expansion. */
static uint32_t des_f(uint32_t r, uint64_t subkey, uint32_t salt_mask)
{
	uint64_t e = des_permute(r, des_e, 48, 32);
	uint32_t swap = (uint32_t)(e ^ (e >> 24)) & salt_mask, out = 0;
	unsigned int i, six;

	e ^= ((uint64_t)swap << 24) | swap;
	e ^= subkey;
	for (i = 0; i < 8; i++) {
		six = (unsigned int)(e >> (42 - 6 * i)) & 0x3f;
		/* outer two bits: row of the S-box, inner four: column */
		out = (out << 4) | des_sbox[i][(six & 0x20) | ((six & 1) << 4) |
					       ((six >> 1) & 0xf)];
	}
	return (uint32_t)des_permute(out, des_p, 32, 32);
}

/* Returns false if the setting does not start with two salt characters. */
static bool des_crypt(const char *key, const char *setting, char *out)
{
	uint32_t salt = 0, salt_mask = 0, c, d, l = 0, r = 0, t;
	uint64_t subkey[16], k = 0, block;
	const char *digit;
	unsigned int i, j;

	/* Two salt characters of six bits each, least significant first. */
	for (i = 0; i < 2; i++) {
		digit = setting[i] ? strchr(crypt_b64, setting[i]) : NULL;
		if (!digit)
			return false;
		salt |= (uint32_t)(digit - crypt_b64) << (6 * i);
	}
	/*
	 * Salt bit i swaps bits i and i + 24 of the 48 bit E expansion,
	 * counted from the most significant end.  This mask selects them in
	 * the low half.
	 */
	for (i = 0; i < 12; i++)
		salt_mask |= ((salt >> i) & 1) << (23 - i);

	/* The key is seven bits of each of the first eight characters. */
	for (i = 0; i < 8; i++) {
		k = (k << 8) | (uint8_t)((unsigned char)*key << 1);
		if (*key)
			key++;
	}
	k = des_permute(k, des_pc1, 56, 64);
	c = (uint32_t)(k >> 28);
	d = (uint32_t)k & 0xfffffff;
	for (i = 0; i < 16; i++) {
		c = rol28(c, des_shifts[i]);
		d = rol28(d, des_shifts[i]);
		subkey[i] = des_permute(((uint64_t)c << 28) | d, des_pc2, 48,
					56);
	}

	for (i = 0; i < 25; i++) {
		for (j = 0; j < 16; j++) {
			t = r;
			r = l ^ des_f(r, subkey[j], salt_mask);
			l = t;
		}
		/* DES ends a pass by swapping the two halves. */
		t = l;
		l = r;
		r = t;
	}
	block = des_permute(((uint64_t)l << 32) | r, des_fp, 64, 64);

	out[0] = setting[0];
	out[1] = setting[1];
	/* Six bits per digit from the top; the last one only has four. */
	for (i = 2; i < 13; i++) {
		out[i] = crypt_b64[block >> 58];
		block <<= 6;
	}
	out[13] = '\0';
	return true;
}

/*
 * As crypt(3): the hash of key in a static buffer, made the way the start
 * of salt says, which may be a whole hash to check key against.
 */
char *crypt(const char *key, const char *salt)
{
	static char result[CRYPT_OUT_LEN];
	char out[CRYPT_OUT_LEN];
	bool ok = true;

	if (!key || !salt)
		ok = false;
	else if (salt[0] != '$')
		ok = des_crypt(key, salt, out);
	else if (!strncmp(salt, "$1$", 3))
		md5_crypt(key, salt, out);
	else if (!strncmp(salt, "$5$", 3))
		sha_crypt(&sha256, key, salt, out);
	else if (!strncmp(salt, "$6$", 3))
		sha_crypt(&sha512, key, salt, out);
	else
		ok = false;

	if (!ok) {
		errno = EINVAL;
		return NULL;
	}

	memcpy(result, out, strlen(out) + 1);
	return result;
}
