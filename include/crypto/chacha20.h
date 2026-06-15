// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>

#pragma once

#include <stddef.h>
#include <stdint.h>

#define CHACHA20_STATE_WORDS ((size_t)16)
#define CHACHA20_BLOCK_SIZE  ((size_t)64)
#define CHACHA20_KEY_SIZE    ((size_t)32)
#define CHACHA20_NONCE_SIZE  ((size_t)12)

typedef struct chacha20_t chacha20_t;

typedef enum chacha20_err_t {
    CHACHA20_OK = 0,
    CHACHA20_ERR_COUNTER_OVF,
} chacha20_err_t;

chacha20_t chacha20_create(const uint8_t key[32], const uint8_t nonce[12],
                           uint32_t counter);

chacha20_err_t chacha20_encrypt(chacha20_t *ctx, size_t len,
                                const uint8_t in[static len],
                                uint8_t out[static len]);

void chacha20_selftest();
