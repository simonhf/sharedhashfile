#ifndef __MURMURHASH3_H__
#define __MURMURHASH3_H__

#include <stdint.h>

extern void MurmurHash3_x64_128(const void * key, const int len, const uint32_t seed, void * out);

#endif /* __MURMURHASH3_H__ */
