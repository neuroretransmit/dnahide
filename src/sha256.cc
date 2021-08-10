#include "sha256.h"

#include <cstring>
#include <iostream>
#include <vector>

#include "binops.h"
#include "types.h"

typedef struct {
    u8 data[64];
    u32 datalen;
    u32 bitlen[2];
    u32 state[8];
} sha256_ctx;

u32 k[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
             0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
             0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
             0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
             0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
             0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
             0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
             0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static inline u32 dbl_int_add(u32 a, u32 b, u32 c)
{
    if (a > 0xffffffff - (c))
        ++b;
    return a += c;
}

static inline u32 ch(u32 x, u32 y, u32 z) { return ((x) & (y)) ^ (~(x) & (z)); }

static inline u32 maj(u32 x, u32 y, u32 z) { return ((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)); }

static u32 inline ep0(u32 x) { return ror(x, 2) ^ ror(x, 13) ^ ror(x, 22); }

static u32 inline ep1(u32 x) { return ror(x, 6) ^ ror(x, 11) ^ ror(x, 25); }

static u32 inline sig0(u32 x) { return ror(x, 7) ^ ror(x, 18) ^ ((x) >> 3); }

static u32 inline sig1(u32 x) { return ror(x, 17) ^ ror(x, 19) ^ ((x) >> 10); }

void transform(sha256_ctx* ctx, u8 data[])
{
    u32 a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for (; i < 64; ++i)
        m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
        t2 = ep0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void init(sha256_ctx* ctx)
{
    ctx->datalen = 0;
    ctx->bitlen[0] = 0;
    ctx->bitlen[1] = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void update(sha256_ctx* ctx, u8 data[], u32 len)
{
    for (u32 i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            transform(ctx, ctx->data);
            dbl_int_add(ctx->bitlen[0], ctx->bitlen[1], 512);
            ctx->datalen = 0;
        }
    }
}

void final(sha256_ctx* ctx, u8 hash[])
{
    u32 i = ctx->datalen;

    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;

        while (i < 56)
            ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;

        while (i < 64)
            ctx->data[i++] = 0x00;

        transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    dbl_int_add(ctx->bitlen[0], ctx->bitlen[1], ctx->datalen * 8);
    ctx->data[63] = ctx->bitlen[0];
    ctx->data[62] = ctx->bitlen[0] >> 8;
    ctx->data[61] = ctx->bitlen[0] >> 16;
    ctx->data[60] = ctx->bitlen[0] >> 24;
    ctx->data[59] = ctx->bitlen[1];
    ctx->data[58] = ctx->bitlen[1] >> 8;
    ctx->data[57] = ctx->bitlen[1] >> 16;
    ctx->data[56] = ctx->bitlen[1] >> 24;
    transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

vector<u8> sha256(char* data)
{
    int str_len = strlen(data);
    sha256_ctx ctx;
    unsigned char hash[32];
    string hash_str = "";

    init(&ctx);
    update(&ctx, (unsigned char*) data, str_len);
    final(&ctx, hash);

    char s[3];
    for (int i = 0; i < 32; i++) {
        sprintf(s, "%02x", hash[i]);
        hash_str += s;
    }

    return vector<uint8_t>(hash, hash + sizeof(hash) / sizeof(uint8_t));
}
