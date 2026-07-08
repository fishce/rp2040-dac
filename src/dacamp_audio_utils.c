#include "dacamp_audio_utils.h"

uint32_t dacamp_pack_stereo_sample(int16_t left, int16_t right)
{
    return ((uint32_t)(uint16_t)right << 16) | (uint16_t)left;
}
