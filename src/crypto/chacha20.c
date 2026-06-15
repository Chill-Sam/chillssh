// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>

#include "crypto/chacha20.h"
#include "log.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32u - (n))))

typedef struct chacha20_t {
    uint32_t state[CHACHA20_STATE_WORDS];
} chacha20_t;

/* Scrub sensitive memory */
static void chacha20_scrub(void *p, size_t n) {
    volatile uint8_t *vp = (volatile uint8_t *)p;
    for (size_t i = 0; i < n; i++) {
        vp[i] = 0;
    }
}

/* Load 4 bytes as a little-endian uint32_t. */
static inline uint32_t load_le32(const uint8_t p[static 4]) {
    assert(p != NULL);
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

/* Store a little-endian uint32_t into 4 bytes. */
static inline void store_le32(uint8_t p[static 4], uint32_t v) {
    assert(p != NULL);
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void chacha20_quarter_round(uint32_t s[static CHACHA20_STATE_WORDS],
                                   const int a, const int b, const int c,
                                   const int d) {
    assert(s != NULL);

    s[a] += s[b];
    s[d] ^= s[a];
    s[d] = ROTL32(s[d], 16);
    s[c] += s[d];
    s[b] ^= s[c];
    s[b] = ROTL32(s[b], 12);
    s[a] += s[b];
    s[d] ^= s[a];
    s[d] = ROTL32(s[d], 8);
    s[c] += s[d];
    s[b] ^= s[c];
    s[b] = ROTL32(s[b], 7);
}

static chacha20_err_t chacha20_block(chacha20_t *cc,
                                     uint8_t out[static CHACHA20_BLOCK_SIZE]) {
    assert(cc != NULL);
    assert(out != NULL);

    if (cc->state[12] == UINT32_MAX) {
        return CHACHA20_ERR_COUNTER_OVF;
    }

    uint32_t w[CHACHA20_STATE_WORDS];
    memcpy(w, cc->state, sizeof(w));

    for (int i = 0; i < 10; i++) {
        /* Column rounds */
        chacha20_quarter_round(w, 0, 4, 8, 12);
        chacha20_quarter_round(w, 1, 5, 9, 13);
        chacha20_quarter_round(w, 2, 6, 10, 14);
        chacha20_quarter_round(w, 3, 7, 11, 15);
        /* Diagonal rounds */
        chacha20_quarter_round(w, 0, 5, 10, 15);
        chacha20_quarter_round(w, 1, 6, 11, 12);
        chacha20_quarter_round(w, 2, 7, 8, 13);
        chacha20_quarter_round(w, 3, 4, 9, 14);
    }

    for (int i = 0; i < 16; i++) {
        w[i] += cc->state[i];
    }

    for (int i = 0; i < 16; i++) {
        store_le32(out + (size_t)i * 4, w[i]);
    }

    chacha20_scrub(w, sizeof(w)); // Scrub w from memory

    cc->state[12]++;

    return CHACHA20_OK;
}

chacha20_t chacha20_create(const uint8_t key[static CHACHA20_KEY_SIZE],
                           const uint8_t nonce[static CHACHA20_NONCE_SIZE],
                           uint32_t counter) {
    assert(key != NULL);
    assert(nonce != NULL);

    chacha20_t chacha;
    chacha.state[0]  = 0x61707865; // expa
    chacha.state[1]  = 0x3320646e; // nd 3
    chacha.state[2]  = 0x79622d32; // 2-by
    chacha.state[3]  = 0x6b206574; // te k

    chacha.state[4]  = load_le32(key + 0);
    chacha.state[5]  = load_le32(key + 4);
    chacha.state[6]  = load_le32(key + 8);
    chacha.state[7]  = load_le32(key + 12);
    chacha.state[8]  = load_le32(key + 16);
    chacha.state[9]  = load_le32(key + 20);
    chacha.state[10] = load_le32(key + 24);
    chacha.state[11] = load_le32(key + 28);

    chacha.state[12] = counter;

    chacha.state[13] = load_le32(nonce + 0);
    chacha.state[14] = load_le32(nonce + 4);
    chacha.state[15] = load_le32(nonce + 8);

    return chacha;
}

chacha20_err_t chacha20_encrypt(chacha20_t *ctx, size_t len,
                                const uint8_t in[static len],
                                uint8_t out[static len]) {
    assert(len > 0);
    assert(ctx != NULL);
    assert(in != NULL);
    assert(out != NULL);

    size_t blocks_needed =
        (len + CHACHA20_BLOCK_SIZE - 1) / CHACHA20_BLOCK_SIZE;
    if (blocks_needed > (size_t)(UINT32_MAX - ctx->state[12])) {
        return CHACHA20_ERR_COUNTER_OVF;
    }

    uint8_t keystream[CHACHA20_BLOCK_SIZE];

    while (len > 0) {
        chacha20_err_t err = chacha20_block(ctx, keystream);
        if (err != CHACHA20_OK) {
            chacha20_scrub(keystream, sizeof(keystream));
            return err;
        }

        size_t n = len < CHACHA20_BLOCK_SIZE ? len : CHACHA20_BLOCK_SIZE;

        for (size_t i = 0; i < n; i++) {
            out[i] = in[i] ^ keystream[i];
        }

        in += n;
        out += n;
        len -= n;
    }

    return CHACHA20_OK;
}

static void test_quarter_round(void) {
    /* RFC 8439 §2.1.1 input */
    uint32_t w[16] = {
        0x11111111u,
        0x01020304u,
        0x9b8d6f43u,
        0x01234567u,
    };

    /* Call the actual QR macro on indices 0,1,2,3 */
    chacha20_quarter_round(w, 0, 1, 2, 3);

    /* RFC 8439 §2.1.1 expected output */
    assert(w[0] == 0xea2a92f4u);
    assert(w[1] == 0xcb1cf8ceu);
    assert(w[2] == 0x4581472eu);
    assert(w[3] == 0x5881c4bbu);
}
static void test_quarter_round_state(void) {
    /* RFC 8439 §2.2.1 input state */
    uint32_t w[16] = {
        0x879531e0u, 0xc5ecf37du, 0x516461b1u, 0xc9a62f8au,
        0x44c20ef3u, 0x3390af7fu, 0xd9fc690bu, 0x2a5f714cu,
        0x53372767u, 0xb00a5631u, 0x974c541au, 0x359e9963u,
        0x5c971061u, 0x3d631689u, 0x2098d9d6u, 0x91dbd320u,
    };

    /* RFC 8439 §2.2.1: apply QUARTERROUND(2, 7, 8, 13) */
    chacha20_quarter_round(w, 2, 7, 8, 13);

    /* RFC 8439 §2.2.1 expected state — only indices 2,7,8,13 changed */
    assert(w[0] == 0x879531e0u);
    assert(w[1] == 0xc5ecf37du);
    assert(w[2] == 0xbdb886dcu); /* changed */
    assert(w[3] == 0xc9a62f8au);
    assert(w[4] == 0x44c20ef3u);
    assert(w[5] == 0x3390af7fu);
    assert(w[6] == 0xd9fc690bu);
    assert(w[7] == 0xcfacafd2u); /* changed */
    assert(w[8] == 0xe46bea80u); /* changed */
    assert(w[9] == 0xb00a5631u);
    assert(w[10] == 0x974c541au);
    assert(w[11] == 0x359e9963u);
    assert(w[12] == 0x5c971061u);
    assert(w[13] == 0xccc07c79u); /* changed */
    assert(w[14] == 0x2098d9d6u);
    assert(w[15] == 0x91dbd320u);
}

static void test_block(void) {
    /* RFC 8439 §2.3.2: key = 00 01 02 ... 1f */
    static const uint8_t key[CHACHA20_KEY_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };

    /* RFC 8439 §2.3.2: nonce = 00 00 00 09 00 00 00 4a 00 00 00 00 */
    static const uint8_t nonce[CHACHA20_NONCE_SIZE] = {
        0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00, 0x00,
    };

    /* RFC 8439 §2.3.2: counter = 1 */
    chacha20_t ctx = chacha20_create(key, nonce, 1);

    uint8_t out[CHACHA20_BLOCK_SIZE];
    chacha20_err_t err = chacha20_block(&ctx, out);
    assert(err == CHACHA20_OK);

    /* RFC 8439 §2.3.2 expected keystream */
    static const uint8_t expected[CHACHA20_BLOCK_SIZE] = {
        0x10, 0xf1, 0xe7, 0xe4, 0xd1, 0x3b, 0x59, 0x15, 0x50, 0x0f, 0xdd,
        0x1f, 0xa3, 0x20, 0x71, 0xc4, 0xc7, 0xd1, 0xf4, 0xc7, 0x33, 0xc0,
        0x68, 0x03, 0x04, 0x22, 0xaa, 0x9a, 0xc3, 0xd4, 0x6c, 0x4e, 0xd2,
        0x82, 0x64, 0x46, 0x07, 0x9f, 0xaa, 0x09, 0x14, 0xc2, 0xd7, 0x05,
        0xd9, 0x8b, 0x02, 0xa2, 0xb5, 0x12, 0x9c, 0xd1, 0xde, 0x16, 0x4e,
        0xb9, 0xcb, 0xd0, 0x83, 0xe8, 0xa2, 0x50, 0x3c, 0x4e,
    };

    for (size_t i = 0; i < CHACHA20_BLOCK_SIZE; i++) {
        assert(out[i] == expected[i]);
    }
}

static void test_encrypt(void) {
    /* RFC 8439 §2.4.2 key */
    static const uint8_t key[CHACHA20_KEY_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };

    /* RFC 8439 §2.4.2 nonce */
    static const uint8_t nonce[CHACHA20_NONCE_SIZE] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00, 0x00,
    };

    /* RFC 8439 §2.4.2 plaintext: "Ladies and Gentlemen of the class
     * of '99: If I could offer you only one tip for the future,
     * sunscreen would be it." */
    static const uint8_t plaintext[] = {
        0x4c, 0x61, 0x64, 0x69, 0x65, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x47,
        0x65, 0x6e, 0x74, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x20, 0x6f, 0x66, 0x20,
        0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x20, 0x6f, 0x66,
        0x20, 0x27, 0x39, 0x39, 0x3a, 0x20, 0x49, 0x66, 0x20, 0x49, 0x20, 0x63,
        0x6f, 0x75, 0x6c, 0x64, 0x20, 0x6f, 0x66, 0x66, 0x65, 0x72, 0x20, 0x79,
        0x6f, 0x75, 0x20, 0x6f, 0x6e, 0x6c, 0x79, 0x20, 0x6f, 0x6e, 0x65, 0x20,
        0x74, 0x69, 0x70, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20,
        0x66, 0x75, 0x74, 0x75, 0x72, 0x65, 0x2c, 0x20, 0x73, 0x75, 0x6e, 0x73,
        0x63, 0x72, 0x65, 0x65, 0x6e, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
        0x62, 0x65, 0x20, 0x69, 0x74, 0x2e,
    };

    /* RFC 8439 §2.4.2 expected ciphertext */
    static const uint8_t expected[] = {
        0x6e, 0x2e, 0x35, 0x9a, 0x25, 0x68, 0xf9, 0x80, 0x41, 0xba, 0x07, 0x28,
        0xdd, 0x0d, 0x69, 0x81, 0xe9, 0x7e, 0x7a, 0xec, 0x1d, 0x43, 0x60, 0xc2,
        0x0a, 0x27, 0xaf, 0xcc, 0xfd, 0x9f, 0xae, 0x0b, 0xf9, 0x1b, 0x65, 0xc5,
        0x52, 0x47, 0x33, 0xab, 0x8f, 0x59, 0x3d, 0xab, 0xcd, 0x62, 0xb3, 0x57,
        0x16, 0x39, 0xd6, 0x24, 0xe6, 0x51, 0x52, 0xab, 0x8f, 0x53, 0x0c, 0x35,
        0x9f, 0x08, 0x61, 0xd8, 0x07, 0xca, 0x0d, 0xbf, 0x50, 0x0d, 0x6a, 0x61,
        0x56, 0xa3, 0x8e, 0x08, 0x8a, 0x22, 0xb6, 0x5e, 0x52, 0xbc, 0x51, 0x4d,
        0x16, 0xcc, 0xf8, 0x06, 0x81, 0x8c, 0xe9, 0x1a, 0xb7, 0x79, 0x37, 0x36,
        0x5a, 0xf9, 0x0b, 0xbf, 0x74, 0xa3, 0x5b, 0xe6, 0xb4, 0x0b, 0x8e, 0xed,
        0xf2, 0x78, 0x5e, 0x42, 0x87, 0x4d,
    };

    assert(sizeof(plaintext) == sizeof(expected));

    chacha20_t ctx = chacha20_create(key, nonce, 1);

    uint8_t out[sizeof(plaintext)];
    chacha20_err_t err =
        chacha20_encrypt(&ctx, sizeof(plaintext), plaintext, out);
    assert(err == CHACHA20_OK);

    for (size_t i = 0; i < sizeof(plaintext); i++) {
        assert(out[i] == expected[i]);
    }
}

void chacha20_selftest() {
    test_quarter_round();
    test_quarter_round_state();
    test_block();
    test_encrypt();

    LOG_INFO("Chacha20 Selftest Success");
}
