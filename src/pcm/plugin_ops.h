/*
 *  Plugin sample operators with fast switch
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <math.h>
#include <stdint.h>

#ifndef SX_INLINES
#define SX_INLINES
static inline uint32_t sx20(uint32_t x)
{
	if(x&0x00080000)
		return x|0xFFF00000;
	return x&0x000FFFFF;
}
static inline uint32_t sx24(uint32_t x)
{
	if(x&0x00800000)
		return x|0xFF000000;
	return x&0x00FFFFFF;
}
static inline uint32_t sx24s(uint32_t x)
{
	if(x&0x00008000)
		return x|0x000000FF;
	return x&0xFFFFFF00;
}
#endif

#define as_u8(ptr) (*(uint8_t*)(ptr))
#define as_u16(ptr) (*(uint16_t*)(ptr))
#define as_u32(ptr) (*(uint32_t*)(ptr))
#define as_u64(ptr) (*(uint64_t*)(ptr))
#define as_s8(ptr) (*(int8_t*)(ptr))
#define as_s16(ptr) (*(int16_t*)(ptr))
#define as_s32(ptr) (*(int32_t*)(ptr))
#define as_s64(ptr) (*(int64_t*)(ptr))
#define as_float(ptr) (*(float_t*)(ptr))
#define as_double(ptr) (*(double_t*)(ptr))

#define as_u8c(ptr) (*(const uint8_t*)(ptr))
#define as_u16c(ptr) (*(const uint16_t*)(ptr))
#define as_u32c(ptr) (*(const uint32_t*)(ptr))
#define as_u64c(ptr) (*(const uint64_t*)(ptr))
#define as_s8c(ptr) (*(const int8_t*)(ptr))
#define as_s16c(ptr) (*(const int16_t*)(ptr))
#define as_s32c(ptr) (*(const int32_t*)(ptr))
#define as_s64c(ptr) (*(const int64_t*)(ptr))
#define as_floatc(ptr) (*(const float_t*)(ptr))
#define as_doublec(ptr) (*(const double_t*)(ptr))

#define _get_triple_le(ptr) (*(uint8_t*)(ptr) | (uint32_t)*((uint8_t*)(ptr) + 1) << 8 | (uint32_t)*((uint8_t*)(ptr) + 2) << 16)
#define _get_triple_be(ptr) ((uint32_t)*(uint8_t*)(ptr) << 16 | (uint32_t)*((uint8_t*)(ptr) + 1) << 8 | *((uint8_t*)(ptr) + 2))
#define _put_triple_le(ptr,val) do { \
	uint8_t *_tmp = (uint8_t *)(ptr); \
	uint32_t _val = (val); \
	_tmp[0] = _val; \
	_tmp[1] = _val >> 8; \
	_tmp[2] = _val >> 16; \
} while(0)
#define _put_triple_be(ptr,val) do { \
	uint8_t *_tmp = (uint8_t *)(ptr); \
	uint32_t _val = (val); \
	_tmp[0] = _val >> 16; \
	_tmp[1] = _val >> 8; \
	_tmp[2] = _val; \
} while(0)

#ifdef SNDRV_LITTLE_ENDIAN
#define _get_triple(ptr) _get_triple_le(ptr)
#define _get_triple_s(ptr) _get_triple_be(ptr)
#define _put_triple(ptr,val) _put_triple_le(ptr,val)
#define _put_triple_s(ptr,val) _put_triple_be(ptr,val)
#else
#define _get_triple(ptr) _get_triple_be(ptr)
#define _get_triple_s(ptr) _get_triple_le(ptr)
#define _put_triple(ptr,val) _put_triple_be(ptr,val)
#define _put_triple_s(ptr,val) _put_triple_le(ptr,val)
#endif

static inline void conv(char *dst, const char *src, unsigned int idx) {
	switch (idx) {
	case   0: /*  8h ->  8h */
	case   1: /*  8h ->  8s */
	case  16: /*  8s ->  8h */
	case  17: /*  8s ->  8s */ as_u8(dst) = as_u8c(src); break;
	case   2: /*  8h -> 16h */
	case  18: /*  8s -> 16h */ as_u16(dst) = (uint16_t)as_u8c(src) << 8; break;
	case   3: /*  8h -> 16s */
	case  19: /*  8s -> 16s */ as_u16(dst) = (uint16_t)as_u8c(src); break;
	case   4: /*  8h -> 24h */
	case  20: /*  8s -> 24h */ as_u32(dst) = sx24((uint32_t)as_u8c(src) << 16); break;
	case   5: /*  8h -> 24s */
	case  21: /*  8s -> 24s */ as_u32(dst) = sx24s((uint32_t)as_u8c(src) << 8); break;
	case   6: /*  8h -> 32h */
	case  22: /*  8s -> 32h */ as_u32(dst) = (uint32_t)as_u8c(src) << 24; break;
	case   7: /*  8h -> 32s */
	case  23: /*  8s -> 32s */ as_u32(dst) = (uint32_t)as_u8c(src); break;
	case   8: /*  8h ^>  8h */
	case   9: /*  8h ^>  8s */
	case  24: /*  8s ^>  8h */
	case  25: /*  8s ^>  8s */ as_u8(dst) = as_u8c(src) ^ 0x80; break;
	case  10: /*  8h ^> 16h */
	case  26: /*  8s ^> 16h */ as_u16(dst) = (uint16_t)(as_u8c(src) ^ 0x80) << 8; break;
	case  11: /*  8h ^> 16s */
	case  27: /*  8s ^> 16s */ as_u16(dst) = (uint16_t)(as_u8c(src) ^ 0x80); break;
	case  12: /*  8h ^> 24h */
	case  28: /*  8s ^> 24h */ as_u32(dst) = sx24((uint32_t)(as_u8c(src) ^ 0x80) << 16); break;
	case  13: /*  8h ^> 24s */
	case  29: /*  8s ^> 24s */ as_u32(dst) = sx24s((uint32_t)(as_u8c(src) ^ 0x80) << 8); break;
	case  14: /*  8h ^> 32h */
	case  30: /*  8s ^> 32h */ as_u32(dst) = (uint32_t)(as_u8c(src) ^ 0x80) << 24; break;
	case  15: /*  8h ^> 32s */
	case  31: /*  8s ^> 32s */ as_u32(dst) = (uint32_t)(as_u8c(src) ^ 0x80); break;
	case  32: /* 16h ->  8h */
	case  33: /* 16h ->  8s */ as_u8(dst) = as_u16c(src) >> 8; break;
	case  34: /* 16h -> 16h */ as_u16(dst) = as_u16c(src); break;
	case  35: /* 16h -> 16s */ as_u16(dst) = bswap_16(as_u16c(src)); break;
	case  36: /* 16h -> 24h */ as_u32(dst) = sx24((uint32_t)as_u16c(src) << 8); break;
	case  37: /* 16h -> 24s */ as_u32(dst) = sx24s((uint32_t)bswap_16(as_u16c(src)) << 8); break;
	case  38: /* 16h -> 32h */ as_u32(dst) = (uint32_t)as_u16c(src) << 16; break;
	case  39: /* 16h -> 32s */ as_u32(dst) = (uint32_t)bswap_16(as_u16c(src)); break;
	case  40: /* 16h ^>  8h */
	case  41: /* 16h ^>  8s */ as_u8(dst) = (as_u16c(src) >> 8) ^ 0x80; break;
	case  42: /* 16h ^> 16h */
	case  51: /* 16s -> 16s */ as_u16(dst) = as_u16c(src) ^ 0x8000; break;
	case  43: /* 16h ^> 16s */
	case  50: /* 16s -> 16h */ as_u16(dst) = bswap_16(as_u16c(src)) ^ 0x80; break;
	case  44: /* 16h ^> 24h */ as_u32(dst) = sx24((uint32_t)(as_u16c(src) ^ 0x8000) << 8); break;
	case  45: /* 16h ^> 24s */ as_u32(dst) = sx24s((uint32_t)(bswap_16(as_u16c(src)) ^ 0x80) << 8); break;
	case  46: /* 16h ^> 32h */ as_u32(dst) = (uint32_t)(as_u16c(src) ^ 0x8000) << 16; break;
	case  47: /* 16h ^> 32s */ as_u32(dst) = (uint32_t)(bswap_16(as_u16c(src)) ^ 0x80); break;
	case  48: /* 16s ->  8h */
	case  49: /* 16s ->  8s */ as_u8(dst) = as_u16c(src) & 0xff; break;
	case  52: /* 16s -> 24h */ as_u32(dst) = sx24((uint32_t)bswap_16(as_u16c(src)) << 8); break;
	case  53: /* 16s -> 24s */ as_u32(dst) = sx24s((uint32_t)as_u16c(src) << 8); break;
	case  54: /* 16s -> 32h */ as_u32(dst) = (uint32_t)bswap_16(as_u16c(src)) << 16; break;
	case  55: /* 16s -> 32s */ as_u32(dst) = (uint32_t)as_u16c(src); break;
	case  56: /* 16s ^>  8h */
	case  57: /* 16s ^>  8s */ as_u8(dst) = (as_u16c(src) ^ 0x80) & 0xff; break;
	case  58: /* 16s ^> 16h */ as_u16(dst) = bswap_16(as_u16c(src) ^ 0x80); break;
	case  59: /* 16s ^> 16s */ as_u16(dst) = as_u16c(src) ^ 0x80; break;
	case  60: /* 16s ^> 24h */ as_u32(dst) = sx24((uint32_t)bswap_16(as_u16c(src) ^ 0x80) << 8); break;
	case  61: /* 16s ^> 24s */ as_u32(dst) = sx24s((uint32_t)(as_u16c(src) ^ 0x80) << 8); break;
	case  62: /* 16s ^> 32h */ as_u32(dst) = (uint32_t)bswap_16(as_u16c(src) ^ 0x80) << 16; break;
	case  63: /* 16s ^> 32s */ as_u32(dst) = (uint32_t)(as_u16c(src) ^ 0x80); break;
	case  64: /* 24h ->  8h */
	case  65: /* 24h ->  8s */ as_u8(dst) = as_u32c(src) >> 16; break;
	case  66: /* 24h -> 16h */ as_u16(dst) = as_u32c(src) >> 8; break;
	case  67: /* 24h -> 16s */ as_u16(dst) = bswap_16(as_u32c(src) >> 8); break;
	case  68: /* 24h -> 24h */ as_u32(dst) = sx24(as_u32c(src)); break;
	case  69: /* 24h -> 24s */ as_u32(dst) = sx24s(bswap_32(as_u32c(src))); break;
	case  70: /* 24h -> 32h */ as_u32(dst) = as_u32c(src) << 8; break;
	case  71: /* 24h -> 32s */ as_u32(dst) = bswap_32(as_u32c(src)) >> 8; break;
	case  72: /* 24h ^>  8h */
	case  73: /* 24h ^>  8s */ as_u8(dst) = (as_u32c(src) >> 16) ^ 0x80; break;
	case  74: /* 24h ^> 16h */ as_u16(dst) = (as_u32c(src) >> 8) ^ 0x8000; break;
	case  75: /* 24h ^> 16s */ as_u16(dst) = bswap_16(as_u32c(src) >> 8) ^ 0x80; break;
	case  76: /* 24h ^> 24h */ as_u32(dst) = sx24(as_u32c(src) ^ 0x800000); break;
	case  77: /* 24h ^> 24s */ as_u32(dst) = sx24s(bswap_32(as_u32c(src)) ^ 0x8000); break;
	case  78: /* 24h ^> 32h */ as_u32(dst) = (as_u32c(src) ^ 0x800000) << 8; break;
	case  79: /* 24h ^> 32s */ as_u32(dst) = (bswap_32(as_u32c(src)) >> 8) ^ 0x80; break;
	case  80: /* 24s ->  8h */
	case  81: /* 24s ->  8s */ as_u8(dst) = (as_u32c(src) >> 8) & 0xff; break;
	case  82: /* 24s -> 16h */ as_u16(dst) = bswap_16(as_u32c(src) >> 8); break;
	case  83: /* 24s -> 16s */ as_u16(dst) = (as_u32c(src) >> 8) & 0xffff; break;
	case  84: /* 24s -> 24h */ as_u32(dst) = sx24(bswap_32(as_u32c(src))); break;
	case  85: /* 24s -> 24s */ as_u32(dst) = sx24s(as_u32c(src)); break;
	case  86: /* 24s -> 32h */ as_u32(dst) = bswap_32(as_u32c(src)) << 8; break;
	case  87: /* 24s -> 32s */ as_u32(dst) = as_u32c(src) >> 8; break;
	case  88: /* 24s ^>  8h */
	case  89: /* 24s ^>  8s */ as_u8(dst) = ((as_u32c(src) >> 8) & 0xff) ^ 0x80; break;
	case  90: /* 24s ^> 16h */ as_u16(dst) = bswap_16((as_u32c(src) >> 8) ^ 0x80); break;
	case  91: /* 24s ^> 16s */ as_u16(dst) = ((as_u32c(src) >> 8) & 0xffff) ^ 0x80; break;
	case  92: /* 24s ^> 24h */ as_u32(dst) = sx24(bswap_32(as_u32c(src)) ^ 0x800000); break;
	case  93: /* 24s ^> 24s */ as_u32(dst) = sx24s(as_u32c(src) ^ 0x8000); break;
	case  94: /* 24s ^> 32h */ as_u32(dst) = bswap_32(as_u32c(src) ^ 0x8000) << 8; break;
	case  95: /* 24s ^> 32s */ as_u32(dst) = (as_u32c(src) >> 8) ^ 0x80; break;
	case  96: /* 32h ->  8h */
	case  97: /* 32h ->  8s */ as_u8(dst) = as_u32c(src) >> 24; break;
	case  98: /* 32h -> 16h */ as_u16(dst) = as_u32c(src) >> 16; break;
	case  99: /* 32h -> 16s */ as_u16(dst) = bswap_16(as_u32c(src) >> 16); break;
	case 100: /* 32h -> 24h */ as_u32(dst) = sx24(as_u32c(src) >> 8); break;
	case 101: /* 32h -> 24s */ as_u32(dst) = sx24s(bswap_32(as_u32c(src)) << 8); break;
	case 102: /* 32h -> 32h */
	case 119: /* 32s -> 32s */ as_u32(dst) = as_u32c(src); break;
	case 103: /* 32h -> 32s */
	case 118: /* 32s -> 32h */ as_u32(dst) = bswap_32(as_u32c(src)); break;
	case 104: /* 32h ^>  8h */
	case 105: /* 32h ^>  8s */ as_u8(dst) = (as_u32c(src) >> 24) ^ 0x80; break;
	case 106: /* 32h ^> 16h */ as_u16(dst) = (as_u32c(src) >> 16) ^ 0x8000; break;
	case 107: /* 32h ^> 16s */ as_u16(dst) = bswap_16(as_u32c(src) >> 16) ^ 0x80; break;
	case 108: /* 32h ^> 24h */ as_u32(dst) = sx24((as_u32c(src) >> 8) ^ 0x800000); break;
	case 109: /* 32h ^> 24s */ as_u32(dst) = sx24s((bswap_32(as_u32c(src)) ^ 0x80) << 8); break;
	case 110: /* 32h ^> 32h */ as_u32(dst) = as_u32c(src) ^ 0x80000000; break;
	case 111: /* 32h ^> 32s */ as_u32(dst) = bswap_32(as_u32c(src)) ^ 0x80; break;
	case 112: /* 32s ->  8h */
	case 113: /* 32s ->  8s */ as_u8(dst) = as_u32c(src) & 0xff; break;
	case 114: /* 32s -> 16h */ as_u16(dst) = bswap_16(as_u32c(src)); break;
	case 115: /* 32s -> 16s */ as_u16(dst) = as_u32c(src) & 0xffff; break;
	case 116: /* 32s -> 24h */ as_u32(dst) = sx24(bswap_32(as_u32c(src)) >> 8); break;
	case 117: /* 32s -> 24s */ as_u32(dst) = sx24s(as_u32c(src) << 8); break;
	case 120: /* 32s ^>  8h */
	case 121: /* 32s ^>  8s */ as_u8(dst) = (as_u32c(src) & 0xff) ^ 0x80; break;
	case 122: /* 32s ^> 16h */ as_u16(dst) = bswap_16(as_u32c(src) ^ 0x80); break;
	case 123: /* 32s ^> 16s */ as_u16(dst) = (as_u32c(src) & 0xffff) ^ 0x80; break;
	case 124: /* 32s ^> 24h */ as_u32(dst) = sx24((bswap_32(as_u32c(src)) >> 8) ^ 0x800000); break;
	case 125: /* 32s ^> 24s */ as_u32(dst) = sx24s((as_u32c(src) ^ 0x80) << 8); break;
	case 126: /* 32s ^> 32h */ as_u32(dst) = bswap_32(as_u32c(src) ^ 0x80); break;
	case 127: /* 32s ^> 32s */ as_u32(dst) = as_u32c(src) ^ 0x80; break;
	}
}

static inline uint16_t get16(const char *src, unsigned int idx) {
	switch(idx) {
	case  0: /*  8h -> 16h */
	case  2: /*  8s -> 16h */ return (uint16_t)as_u8c(src) << 8;
	case  1: /*  8h ^> 16h */
	case  3: /*  8s ^> 16h */ return (uint16_t)(as_u8c(src) ^ 0x80) << 8;
	case  4: /* 16h -> 16h */ return as_u16c(src);
	case  5: /* 16h ^> 16h */ return as_u16c(src) ^ 0x8000;
	case  6: /* 16s -> 16h */ return bswap_16(as_u16c(src));
	case  7: /* 16s ^> 16h */ return bswap_16(as_u16c(src) ^ 0x80);
	/* 4 byte formats */
	case  8: /* 24h -> 16h */ return as_u32c(src) >> 8;
	case  9: /* 24h ^> 16h */ return (as_u32c(src) >> 8) ^ 0x8000;
	case 10: /* 24s -> 16h */ return bswap_16(as_u32c(src) >> 8);
	case 11: /* 24s ^> 16h */ return bswap_16((as_u32c(src) >> 8) ^ 0x80);
	case 12: /* 32h -> 16h */ return as_u32c(src) >> 16;
	case 13: /* 32h ^> 16h */ return (as_u32c(src) >> 16) ^ 0x8000;
	case 14: /* 32s -> 16h */ return bswap_16(as_u32c(src));
	case 15: /* 32s ^> 16h */ return bswap_16(as_u32c(src) ^ 0x80);
	case 16: /* 20h -> 16h */ return as_u32c(src) >> 4;
	case 17: /* 20h ^> 16h */ return (as_u32c(src) >> 4) ^ 0x8000;
	case 18: /* 20s -> 16h */ return bswap_32(as_u32c(src)) >> 4;
	case 19: /* 20s ^> 16h */ return (bswap_32(as_u32c(src)) >> 4) ^ 0x8000;
	/* 3bytes format */
	case 20: /* 24h -> 16h */ return _get_triple(src) >> 8;
	case 21: /* 24h ^> 16h */ return (_get_triple(src) >> 8) ^ 0x8000;
	case 22: /* 24s -> 16h */ return _get_triple_s(src) >> 8;
	case 23: /* 24s ^> 16h */ return (_get_triple_s(src) >> 8) ^ 0x8000;
	case 24: /* 20h -> 16h */ return _get_triple(src) >> 4;
	case 25: /* 20h ^> 16h */ return (_get_triple(src) >> 4) ^ 0x8000;
	case 26: /* 20s -> 16h */ return _get_triple_s(src) >> 4;
	case 27: /* 20s ^> 16h */ return (_get_triple_s(src) >> 4) ^ 0x8000;
	case 28: /* 18h -> 16h */ return _get_triple(src) >> 2;
	case 29: /* 18h ^> 16h */ return (_get_triple(src) >> 2) ^ 0x8000;
	case 30: /* 18s -> 16h */ return _get_triple_s(src) >> 2;
	case 31: /* 18s ^> 16h */ return (_get_triple_s(src) >> 2) ^ 0x8000;
	}
}

static inline void put16(char *dst, int16_t sample, unsigned int idx)
{
	switch (idx) {
	case  0: /* 16h ->  8h */
	case  2: /* 16h ->  8s */ as_u8(dst) = sample >> 8; break;
	case  1: /* 16h ^>  8h */
	case  3: /* 16h ^>  8s */ as_u8(dst) = (sample >> 8) ^ 0x80; break;
	case  4: /* 16h -> 16h */ as_u16(dst) = sample; break;
	case  5: /* 16h ^> 16h */ as_u16(dst) = sample ^ 0x8000; break;
	case  6: /* 16h -> 16s */ as_u16(dst) = bswap_16(sample); break;
	case  7: /* 16h ^> 16s */ as_u16(dst) = bswap_16(sample) ^ 0x80; break;
	/* 4 byte formats */
	case  8: /* 16h -> 24h */ as_u32(dst) = sx24((uint32_t)sample << 8); break;
	case  9: /* 16h ^> 24h */ as_u32(dst) = sx24((uint32_t)(sample ^ 0x8000) << 8); break;
	case 10: /* 16h -> 24s */ as_u32(dst) = sx24s((uint32_t)bswap_16(sample) << 8); break;
	case 11: /* 16h ^> 24s */ as_u32(dst) = sx24s((uint32_t)(bswap_16(sample) ^ 0x80) << 8); break;
	case 12: /* 16h -> 32h */ as_u32(dst) = (uint32_t)sample << 16; break;
	case 13: /* 16h ^> 32h */ as_u32(dst) = (uint32_t)(sample ^ 0x8000) << 16; break;
	case 14: /* 16h -> 32s */ as_u32(dst) = (uint32_t)bswap_16(sample); break;
	case 15: /* 16h ^> 32s */ as_u32(dst) = (uint32_t)bswap_16(sample) ^ 0x80; break;
	case 16: /* 16h -> 20h */ as_u32(dst) = sx20((uint32_t)sample << 4); break;
	case 17: /* 16h ^> 20h */ as_u32(dst) = sx20((uint32_t)(sample ^ 0x8000) << 4); break;
	case 18: /* 16h -> 20s */ as_u32(dst) = bswap_32(sx20((uint32_t)sample << 4)); break;
	case 19: /* 16h ^> 20s */ as_u32(dst) = bswap_32(sx20((uint32_t)(sample ^ 0x8000) << 4)); break;
	/* 3bytes format */
	case 20: /* 16h -> 24h */ _put_triple(dst, (uint32_t)sample << 8); break;
	case 21: /* 16h ^> 24h */ _put_triple(dst, (uint32_t)(sample ^ 0x8000) << 8); break;
	case 22: /* 16h -> 24s */ _put_triple_s(dst, (uint32_t)sample << 8); break;
	case 23: /* 16h ^> 24s */ _put_triple_s(dst, (uint32_t)(sample ^ 0x8000) << 8); break;
	case 24: /* 16h -> 20h */ _put_triple(dst, (uint32_t)sample << 4); break;
	case 25: /* 16h ^> 20h */ _put_triple(dst, (uint32_t)(sample ^ 0x8000) << 4); break;
	case 26: /* 16h -> 20s */ _put_triple_s(dst, (uint32_t)sample << 4); break;
	case 27: /* 16h ^> 20s */ _put_triple_s(dst, (uint32_t)(sample ^ 0x8000) << 4); break;
	case 28: /* 16h -> 18h */ _put_triple(dst, (uint32_t)sample << 2); break;
	case 29: /* 16h ^> 18h */ _put_triple(dst, (uint32_t)(sample ^ 0x8000) << 2); break;
	case 30: /* 16h -> 18s */ _put_triple_s(dst, (uint32_t)sample << 2); break;
	case 31: /* 16h ^> 18s */ _put_triple_s(dst, (uint32_t)(sample ^ 0x8000) << 2); break;
	}
}

static inline int32_t get32(const char *src, unsigned int idx)
{
	switch (idx) {
	case  0: /*  8h -> 32h */
	case  2: /*  8s -> 32h */ return (uint32_t)as_u8c(src) << 24;
	case  1: /*  8h ^> 32h */
	case  3: /*  8s ^> 32h */ return (uint32_t)(as_u8c(src) ^ 0x80) << 24;
	case  4: /* 16h -> 32h */ return (uint32_t)as_u16c(src) << 16;
	case  5: /* 16h ^> 32h */ return (uint32_t)(as_u16c(src) ^ 0x8000) << 16;
	case  6: /* 16s -> 32h */ return (uint32_t)bswap_16(as_u16c(src)) << 16;
	case  7: /* 16s ^> 32h */ return (uint32_t)bswap_16(as_u16c(src) ^ 0x80) << 16;
	/* 4 byte formats */
	case  8: /* 24h -> 32h */ return as_u32c(src) << 8;
	case  9: /* 24h ^> 32h */ return (as_u32c(src) << 8) ^ 0x80000000;
	case 10: /* 24s -> 32h */ return bswap_32(as_u32c(src) >> 8);
	case 11: /* 24s ^> 32h */ return bswap_32((as_u32c(src) >> 8) ^ 0x80);
	case 12: /* 32h -> 32h */ return as_u32c(src);
	case 13: /* 32h ^> 32h */ return as_u32c(src) ^ 0x80000000;
	case 14: /* 32s -> 32h */ return bswap_32(as_u32c(src));
	case 15: /* 32s ^> 32h */ return bswap_32(as_u32c(src) ^ 0x80);
	case 16: /* 20h -> 32h */ return as_u32c(src) << 12;
	case 17: /* 20h ^> 32h */ return (as_u32c(src) << 12) ^ 0x80000000;
	case 18: /* 20s -> 32h */ return bswap_32(as_u32c(src)) << 12;
	case 19: /* 20s ^> 32h */ return (bswap_32(as_u32c(src)) << 12) ^ 0x80000000;
	/* 3bytes format */
	case 20: /* 24h -> 32h */ return _get_triple(src) << 8;
	case 21: /* 24h ^> 32h */ return (_get_triple(src) << 8) ^ 0x80000000;
	case 22: /* 24s -> 32h */ return _get_triple_s(src) << 8;
	case 23: /* 24s ^> 32h */ return (_get_triple_s(src) << 8) ^ 0x80000000;
	case 24: /* 20h -> 32h */ return _get_triple(src) << 12;
	case 25: /* 20h ^> 32h */ return (_get_triple(src) << 12) ^ 0x80000000;
	case 26: /* 20s -> 32h */ return _get_triple_s(src) << 12;
	case 27: /* 20s ^> 32h */ return (_get_triple_s(src) << 12) ^ 0x80000000;
	case 28: /* 18h -> 32h */ return _get_triple(src) << 14;
	case 29: /* 18h ^> 32h */ return (_get_triple(src) << 14) ^ 0x80000000;
	case 30: /* 18s -> 32h */ return _get_triple_s(src) << 14;
	case 31: /* 18s ^> 32h */ return (_get_triple_s(src) << 14) ^ 0x80000000;
	}
}

static inline void put32(char *dst, int32_t sample, unsigned int idx)
{
	switch (idx) {
	case  0: /* 32h ->  8h */
	case  2: /* 32h ->  8s */ as_u8(dst) = sample >> 24; break;
	case  1: /* 32h ^>  8h */
	case  3: /* 32h ^>  8s */ as_u8(dst) = (sample >> 24) ^ 0x80; break;
	case  4: /* 32h -> 16h */ as_u16(dst) = sample >> 16; break;
	case  5: /* 32h ^> 16h */ as_u16(dst) = (sample >> 16) ^ 0x8000; break;
	case  6: /* 32h -> 16s */ as_u16(dst) = bswap_16(sample >> 16); break;
	case  7: /* 32h ^> 16s */ as_u16(dst) = bswap_16(sample >> 16) ^ 0x80; break;
	/* 4 byte formats */
	case  8: /* 32h -> 24h */ as_u32(dst) = sx24(sample >> 8); break;
	case  9: /* 32h ^> 24h */ as_u32(dst) = sx24((sample >> 8) ^ 0x800000); break;
	case 10: /* 32h -> 24s */ as_u32(dst) = sx24s(bswap_32(sample) << 8); break;
	case 11: /* 32h ^> 24s */ as_u32(dst) = sx24s((bswap_32(sample) ^ 0x80) << 8); break;
	case 12: /* 32h -> 32h */ as_u32(dst) = sample; break;
	case 13: /* 32h ^> 32h */ as_u32(dst) = sample ^ 0x80000000; break;
	case 14: /* 32h -> 32s */ as_u32(dst) = bswap_32(sample); break;
	case 15: /* 32h ^> 32s */ as_u32(dst) = bswap_32(sample) ^ 0x80; break;
	case 16: /* 32h -> 20h */ as_u32(dst) = sx20(sample >> 12); break;
	case 17: /* 32h ^> 20h */ as_u32(dst) = sx20((sample ^ 0x80000000) >> 12); break;
	case 18: /* 32h -> 20s */ as_u32(dst) = bswap_32(sx20(sample >> 12)); break;
	case 19: /* 32h ^> 20s */ as_u32(dst) = bswap_32(sx20((sample ^ 0x80000000) >> 12)); break;
	/* 3bytes format */
	case 20: /* 32h -> 24h */ _put_triple(dst, sample >> 8); break;
	case 21: /* 32h ^> 24h */ _put_triple(dst, (sample ^ 0x80000000) >> 8); break;
	case 22: /* 32h -> 24s */ _put_triple_s(dst, sample >> 8); break;
	case 23: /* 32h ^> 24s */ _put_triple_s(dst, (sample ^ 0x80000000) >> 8); break;
	case 24: /* 32h -> 20h */ _put_triple(dst, sample >> 12); break;
	case 25: /* 32h ^> 20h */ _put_triple(dst, (sample ^ 0x80000000) >> 12); break;
	case 26: /* 32h -> 20s */ _put_triple_s(dst, sample >> 12); break;
	case 27: /* 32h ^> 20s */ _put_triple_s(dst, (sample ^ 0x80000000) >> 12); break;
	case 28: /* 32h -> 18h */ _put_triple(dst, sample >> 14); break;
	case 29: /* 32h ^> 18h */ _put_triple(dst, (sample ^ 0x80000000) >> 14); break;
	case 30: /* 32h -> 18s */ _put_triple_s(dst, sample >> 14); break;
	case 31: /* 32h ^> 18s */ _put_triple_s(dst, (sample ^ 0x80000000) >> 14); break;
	}
}

static inline void put32float(char *dst, int32_t sample, unsigned int idx)
{
	snd_tmp_float_t tmp_float;
	snd_tmp_double_t tmp_double;

	switch (idx) {
	case 0: /* 32h -> (float)h */
		as_float(dst) = (float_t)((int32_t)sample) / (float_t)0x80000000UL; break;
	case 1: /* 32h -> (float)s */
		tmp_float.f = (float_t)((int32_t)sample) / (float_t)0x80000000UL;
		as_u32(dst) = bswap_32(tmp_float.i); break;
	case 2: /* 32h -> (float64)h */
		as_double(dst) = (double_t)((int32_t)sample) / (double_t)0x80000000UL; break;
	case 3: /* 32h -> (float64)s */
		tmp_double.d = (double_t)((int32_t)sample) / (double_t)0x80000000UL;
		as_u64(dst) = bswap_64(tmp_double.l); break;
	}
}

static inline int32_t get32float(const char *src, unsigned int idx)
{
	snd_tmp_float_t tmp_float;
	snd_tmp_double_t tmp_double;

	switch (idx) {
	case 0: /* (float)h -> 32h */
		tmp_float.f = as_floatc(src);
		if (tmp_float.f >= 1.0)
			return 0x7fffffff;
		if (tmp_float.f <= -1.0)
			return 0x80000000;
		return (int32_t)(tmp_float.f * (float_t)0x80000000UL);
	case 1: /* (float)s -> 32h */
		tmp_float.i = bswap_32(as_u32c(src));
		if (tmp_float.f >= 1.0)
			return 0x7fffffff;
		if (tmp_float.f <= -1.0)
			return 0x80000000;
		return (int32_t)(tmp_float.f * (float_t)0x80000000UL);
	case 2: /* (float64)h -> 32h */
		tmp_double.d = as_doublec(src);
		if (tmp_double.d >= 1.0)
			return 0x7fffffff;
		if (tmp_double.d <= -1.0)
			return 0x80000000;
		return (int32_t)(tmp_double.d * (double_t)0x80000000UL);
	case 3: /* (float64)s -> 32h */
		tmp_double.l = bswap_64(as_u64c(src));
		if (tmp_double.d >= 1.0)
			return 0x7fffffff;
		if (tmp_double.d <= -1.0)
			return 0x80000000;
		return (int32_t)(tmp_double.d * (double_t)0x80000000UL);
	}
}

#undef as_u8
#undef as_u16
#undef as_u32
#undef as_s8
#undef as_s16
#undef as_s32
#undef as_float
#undef as_double

#undef as_u8c
#undef as_u16c
#undef as_u32c
#undef as_s8c
#undef as_s16c
#undef as_s32c
#undef as_floatc
#undef as_doublec

#undef _get_triple
#undef _get_triple_s
#undef _get_triple_le
#undef _get_triple_be
#undef _put_triple
#undef _put_triple_s
#undef _put_triple_le
#undef _put_triple_be

