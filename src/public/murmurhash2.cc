
#include "murmurhash2.h"

uint32_t mur_mur_hash2( const void * key, int32_t len, uint32_t seed )
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.

    const uint32_t m = 0x5bd1e995;
    const int32_t r = 24;

    // Initialize the hash to a 'random' value

    uint32_t h = seed ^ len;

    // Mix 4 bytes at a time into the hash

    const uint8_t* data = (const uint8_t*)key;

    while(len >= 4)
    {
		uint32_t k = *(uint32_t*)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
    }

    // Handle the last few bytes of the input array

	const int8_t* idata = (const int8_t*)data;
	switch(len)
	{
		case 3: h ^= idata[2] << 16;
		case 2: h ^= idata[1] << 8;
		case 1: h ^= idata[0];
		h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}
