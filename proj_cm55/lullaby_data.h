/*******************************************************************************
* lullaby_data.h — Full-quality music-box lullaby, 48 kHz mono s16le.
* Array lives in unallocated HyperFlash (.cy_lullaby_pcm @ 0x60C00000).
* XIP-readable by CM55 — does not consume CM55 code-partition flash.
*******************************************************************************/
#ifndef LULLABY_DATA_H_
#define LULLABY_DATA_H_

#include <stdint.h>

extern const uint32_t lullaby_pcm_samples;  /* sample count at 48 kHz */
extern const int16_t  lullaby_pcm_data[];   /* 2 MB in ext flash      */

#endif /* LULLABY_DATA_H_ */
