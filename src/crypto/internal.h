// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>

#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* Scrub sensitive memory */
static inline void scrub(void *p, size_t n) {
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
