#include "pcm5102a_output.h"

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "dacamp_audio_utils.h"
#include "i2s_tx.pio.h"

/* Common PCM5102A breakout wiring:
 *   BCK -> GPIO 18
 *   LCK -> GPIO 19
 *   DIN -> GPIO 20
 * If your board uses a different mapping, change these three values only.
 */
#ifndef PCM5102A_GPIO_BCLK
#define PCM5102A_GPIO_BCLK 18
#endif
#ifndef PCM5102A_GPIO_LRCLK
#define PCM5102A_GPIO_LRCLK 19
#endif
#ifndef PCM5102A_GPIO_DIN
#define PCM5102A_GPIO_DIN 20
#endif

#define I2S_TX_PIN_BCLK  PCM5102A_GPIO_BCLK
#define I2S_TX_PIN_WS    PCM5102A_GPIO_LRCLK
#define I2S_TX_PIN_DIN   PCM5102A_GPIO_DIN
#define I2S_TX_PIN_LRCLK PCM5102A_GPIO_LRCLK

#define I2S_DMA_BUFFER_SAMPLES 256
#define I2S_DMA_BUFFER_WORDS (I2S_DMA_BUFFER_SAMPLES * 2)

static uint32_t i2s_dma_buffer0[I2S_DMA_BUFFER_WORDS];
static uint32_t i2s_dma_buffer1[I2S_DMA_BUFFER_WORDS];
static volatile bool i2s_dma_ready;
static volatile bool i2s_running;
static int i2s_dma_channel;
static int i2s_tx_program_offset;
static uint32_t i2s_sample_rate;

static void pcm5102a_dma_handler(void)
{
    if (!i2s_running) return;
    dma_hw->ints0 = 1u << i2s_dma_channel;
    i2s_dma_ready = true;
}

void pcm5102a_output_init(uint32_t sample_rate)
{
    i2s_sample_rate = sample_rate;

    gpio_init(I2S_TX_PIN_BCLK);
    gpio_init(I2S_TX_PIN_WS);
    gpio_init(I2S_TX_PIN_DIN);
    gpio_set_dir(I2S_TX_PIN_BCLK, GPIO_OUT);
    gpio_set_dir(I2S_TX_PIN_WS, GPIO_OUT);
    gpio_set_dir(I2S_TX_PIN_DIN, GPIO_OUT);

    gpio_set_function(I2S_TX_PIN_BCLK, GPIO_FUNC_PIO0);
    gpio_set_function(I2S_TX_PIN_WS, GPIO_FUNC_PIO0);
    gpio_set_function(I2S_TX_PIN_DIN, GPIO_FUNC_PIO0);

    PIO pio = pio0;
    i2s_tx_program_offset = pio_add_program(pio, &i2s_tx_program);
    uint sm = (uint)pio_claim_unused_sm(pio, true);
    pio_sm_config c = i2s_tx_program_get_default_config(i2s_tx_program_offset);
    sm_config_set_out_pins(&c, I2S_TX_PIN_DIN, 1);
    sm_config_set_sideset_pins(&c, I2S_TX_PIN_BCLK);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_clkdiv(&c, 1.0f);
    pio_gpio_init(pio, I2S_TX_PIN_DIN);
    pio_gpio_init(pio, I2S_TX_PIN_WS);
    pio_gpio_init(pio, I2S_TX_PIN_BCLK);
    pio_sm_init(pio, sm, i2s_tx_program_offset, &c);
    pio_sm_set_consecutive_pindirs(pio, sm, I2S_TX_PIN_BCLK, 3, true);
    pio_sm_set_enabled(pio, sm, true);

    i2s_dma_channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(i2s_dma_channel);
    channel_config_set_dreq(&cfg, DREQ_PIO0_TX0 + sm);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    dma_channel_configure(i2s_dma_channel, &cfg,
                          &pio->txf[sm],
                          NULL,
                          I2S_DMA_BUFFER_WORDS,
                          false);

    irq_set_exclusive_handler(DMA_IRQ_0, pcm5102a_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(i2s_dma_channel, true);

    memset(i2s_dma_buffer0, 0, sizeof(i2s_dma_buffer0));
    memset(i2s_dma_buffer1, 0, sizeof(i2s_dma_buffer1));
    i2s_running = false;
}

void pcm5102a_output_start(void)
{
    i2s_running = true;
}

void pcm5102a_output_stop(void)
{
    i2s_running = false;
}

void pcm5102a_output_submit_samples(const int32_t *left, const int32_t *right, size_t sample_count)
{
    if (!i2s_running || sample_count == 0) return;

    static uint32_t *active_buffer = i2s_dma_buffer0;
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t l = left[i];
        int32_t r = right[i];
        int32_t scaled_l = l >> 8;
        int32_t scaled_r = r >> 8;
        if (scaled_l < -0x7fff) scaled_l = -0x7fff;
        if (scaled_l > 0x7fff) scaled_l = 0x7fff;
        if (scaled_r < -0x7fff) scaled_r = -0x7fff;
        if (scaled_r > 0x7fff) scaled_r = 0x7fff;
        active_buffer[i * 2] = dacamp_pack_stereo_sample((int16_t)scaled_l, (int16_t)scaled_r);
        active_buffer[i * 2 + 1] = dacamp_pack_stereo_sample((int16_t)scaled_l, (int16_t)scaled_r);
    }

    dma_channel_set_read_addr(i2s_dma_channel, active_buffer, true);
    active_buffer = (active_buffer == i2s_dma_buffer0) ? i2s_dma_buffer1 : i2s_dma_buffer0;
}
