
#pragma once

#include <string>
#include <stdlib.h>
#include <string.h>

int32_t encodehexstring(const uint8_t *s, int32_t len, uint8_t *sEncoded);
int32_t decodehexstring(const uint8_t *s, int32_t len, uint8_t *sEncoded);

int32_t MD5(const uint8_t *input, uint32_t len, uint8_t *output);
int32_t MD5AndEncode(const uint8_t *input, uint32_t len, uint8_t* output);

