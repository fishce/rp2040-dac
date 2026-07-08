#pragma once

#include <stddef.h>
#include <stdint.h>

void pcm5102a_output_init(uint32_t sample_rate);
void pcm5102a_output_start(void);
void pcm5102a_output_stop(void);
void pcm5102a_output_submit_samples(const int32_t *left, const int32_t *right, size_t sample_count);
