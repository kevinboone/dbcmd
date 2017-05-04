#pragma once

#ifndef uchar
#define uchar uint8_t 
#endif 

#ifndef uint
#define uint uint32_t 
#endif 

#include <stdint.h>

typedef struct {
   uchar data[64];
   uint datalen;
   uint bitlen[2];
   uint state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, uchar data[], uint len);
void sha256_final(SHA256_CTX *ctx, uchar hash[]);
// Hash a block of arbitrary size
void sha256_hash_block (unsigned char *block, int len, unsigned char hash[32]);

