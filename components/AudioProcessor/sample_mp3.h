#pragma once
#include <stdint.h>
#include <stddef.h>

/* First ~30 seconds of the lofi track embedded directly in flash.
 * Source file: components/AudioProcessor/audio_sample.mp3
 *
 * To swap in the full song:
 *   cp "/Users/cachin/mp3/[Non-Copyrighted Music] Chill Jazzy Lofi Hip Hop (Royalty Free) Jazz Hop Music.mp3" \
 *      components/AudioProcessor/audio_sample.mp3
 * Then rebuild. */

extern const uint8_t _binary_audio_sample_mp3_start[];
extern const uint8_t _binary_audio_sample_mp3_end[];

#define kSampleMp3      (_binary_audio_sample_mp3_start)
#define kSampleMp3Size  ((size_t)(_binary_audio_sample_mp3_end - _binary_audio_sample_mp3_start))
