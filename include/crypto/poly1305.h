// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>

#pragma once

#include <stddef.h>
#include <stdint.h>

#define POLY1305_TAG_SIZE 16
#define POLY1305_KEY_SIZE 32

void poly1305_mac(uint8_t tag[POLY1305_TAG_SIZE],
                  const uint8_t key[POLY1305_KEY_SIZE], size_t len,
                  const uint8_t msg[static len]);
void poly1305_selftest(void);
