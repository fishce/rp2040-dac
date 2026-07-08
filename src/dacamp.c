#include "dacamp.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"

#include "dsm.h"
#include "volumeLut.h"
#include "pcm5102a_output.h"

#define PCM5102A_DAC_SAMPLES_PER_FRAME 64

#define PCM_RING_BUFFER_DEPTH 2048

static volatile bool isEnabledRequested = false, isFlushRequested = false;
static volatile uint32_t requestedSampleRate;

#define _DACAMP_PCM16_LEFT(pcm)         ((int16_t)(pcm))
#define _DACAMP_PCM16_RIGHT(pcm)        ((int16_t)((pcm) >> 16))
 
#define _DACAMP_PCM24_LEFT(pcm)         (((int32_t)(pcm)) >> 8)
#define _DACAMP_PCM24_RIGHT(pcm)        ((int32_t)((pcm) >> 32) >> 8)

#define _DACAMP_DSM_PCM_LEFT(pcm)       ((int32_t)(pcm))
#define _DACAMP_DSM_PCM_RIGHT(pcm)      ((int32_t)((pcm) >> 32))
#define _DACAMP_DSM_PCM(left, right)    (((uint64_t)(left & 0xFFFFFFFF)) | (((uint64_t)((right)) << 32)))

static void dacamp_panic(void);
static void dacamp_init_cringe_debug(void);

static void dacamp_push_pcm5102a_output(int32_t left, int32_t right)
{
    static int32_t left_samples[PCM5102A_DAC_SAMPLES_PER_FRAME];
    static int32_t right_samples[PCM5102A_DAC_SAMPLES_PER_FRAME];
    static int frame_index = 0;

    left_samples[frame_index] = left;
    right_samples[frame_index] = right;
    frame_index++;

    if (frame_index == PCM5102A_DAC_SAMPLES_PER_FRAME) {
        pcm5102a_output_submit_samples(left_samples, right_samples, PCM5102A_DAC_SAMPLES_PER_FRAME);
        frame_index = 0;
    }
}

void dacamp_init(void)
{
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1);

    set_sys_clock_pll(1536000000, 4, 2);

    dacamp_init_cringe_debug();
    pcm5102a_output_init(48000);
}

void dacamp_start(uint32_t sampleRate)
{
    requestedSampleRate = sampleRate;
    isEnabledRequested = true;
    pcm5102a_output_start();
}

void dacamp_change_sample_rate(uint32_t sampleRate)
{
    requestedSampleRate = sampleRate;
    dacamp_flush();
}

void dacamp_stop(void)
{
    isEnabledRequested = false;
    isFlushRequested = true;
    pcm5102a_output_stop();
}

void dacamp_flush(void)
{
    isFlushRequested = true;
}

int dacamp_pcm_put(const uint32_t *samples, int sampleCount, int sampleSize, const int16_t *volume, const int8_t *mute)
{
    if (!isEnabledRequested)
        return sampleCount;

    int32_t volumeLeft = (int32_t)volume[0] + (int32_t)volume[1];
    int32_t volumeRight = (int32_t)volume[0] + (int32_t)volume[2];

    int32_t volumeIndexLeft = (-volumeLeft) >> DACAMP_VOLUME_STEP_BITS;
    int32_t volumeIndexRight = (-volumeRight) >> DACAMP_VOLUME_STEP_BITS;

    int32_t muteLeft = mute[0] || mute[1] || volumeLeft <= DACAMP_MIN_VOLUME_UAC2;
    int32_t muteRight = mute[0] || mute[2] || volumeRight <= DACAMP_MIN_VOLUME_UAC2;

    const uint64_t *samples64 = (const uint64_t *)samples;

    for (int i = 0; i < sampleCount; ++i)
    {
        int32_t sampleLeft;
        int32_t sampleRight;

        if (sampleSize == 4)
        {
            uint32_t sample = samples[i];
            sampleLeft = DSM_INT16_TO_INT32((int16_t)(sample & 0xFFFF));
            sampleRight = DSM_INT16_TO_INT32((int16_t)(sample >> 16));
        }
        else
        {
            uint64_t sample = samples64[i];
            sampleLeft = DSM_INT24_TO_INT32((int32_t)((sample & 0xFFFFFFFFu) >> 8));
            sampleRight = DSM_INT24_TO_INT32((int32_t)((sample >> 32) >> 8));
        }

        if (!muteLeft)
            sampleLeft = (sampleLeft * volumeLutNumerator[volumeIndexLeft]) / volumeLutDenominator[volumeIndexLeft];
        else
            sampleLeft = 0;

        if (!muteRight)
            sampleRight = (sampleRight * volumeLutNumerator[volumeIndexRight]) / volumeLutDenominator[volumeIndexRight];
        else
            sampleRight = 0;

        dacamp_push_pcm5102a_output(sampleLeft, sampleRight);
    }

    return sampleCount;
}

static void dacamp_panic(void)
{
    //enable led
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, 1);

    panic("dacamp panic");
}

#define CRINGE_DEBUG_LED1 4
#define CRINGE_DEBUG_LED2 5

static void dacamp_init_cringe_debug(void)
{
    gpio_init(CRINGE_DEBUG_LED1);
    gpio_init(CRINGE_DEBUG_LED2);
    gpio_set_dir(CRINGE_DEBUG_LED1, GPIO_OUT);
    gpio_set_dir(CRINGE_DEBUG_LED2, GPIO_OUT);
    gpio_set_drive_strength(CRINGE_DEBUG_LED1, GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(CRINGE_DEBUG_LED1, GPIO_DRIVE_STRENGTH_2MA);
}

void dacamp_debug_stuff_task(void)
{
    gpio_put(CRINGE_DEBUG_LED1, isEnabledRequested);
    gpio_put(CRINGE_DEBUG_LED2, isFlushRequested);
}