/**
 * MIT License
 *
 * Copyright (c) 2022 Vincent Mistler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "i2s-audio.h"

/**
 * return the default i2s context used to store information about the setup
 * RP2040 的每个 PIO 只能控制自己对应范围的 GPIO。
 * GP9～GP11 属于 PIO0（默认即可）。
 * GP26～GP28 属于 PIO1。
 */
i2s_config_t i2s_get_default_config(void) {
    i2s_config_t i2s_config = {
		.sample_freq = 44100, 
		.channel_count = 2,
		.data_pin = 17,
		.clock_pin_base = 18,
		.pio = pio1,
		.sm = 1,
        .dma_channel = 0,
        .dma_trans_count = 0,
        .dma_buf = (uint16_t*) NULL,
        .volume = 0,
	};

    return i2s_config;
}

/**
 * Initialize the I2S driver. Must be called before calling i2s_write or i2s_dma_write
 * i2s_config: I2S context obtained by i2s_get_default_config()
 */
void i2s_init(i2s_config_t *i2s_config) {
    // 根据PIO选择正确的GPIO功能
    gpio_function_t func = (i2s_config->pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;
    gpio_set_function(i2s_config->data_pin, func);
    gpio_set_function(i2s_config->clock_pin_base, func);
    gpio_set_function(i2s_config->clock_pin_base+1, func);
    
    i2s_config->sm = pio_claim_unused_sm(i2s_config->pio, true);
    
    uint offset = pio_add_program(i2s_config->pio, &audio_i2s_program);

    audio_i2s_program_init(i2s_config->pio, i2s_config->sm , offset, i2s_config->data_pin , i2s_config->clock_pin_base);
    
    /* Set PIO clock.
     * 32-bit slots are 64 BCLK per stereo frame (two PIO cycles per bit), so the
     * divider is half of the original 16-bit-slot program. Matches picoTracker. */
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    uint32_t divider = system_clock_frequency * 2 / i2s_config->sample_freq;
    pio_sm_set_clkdiv_int_frac(i2s_config->pio, i2s_config->sm , divider >> 8u, divider & 0xffu);

    pio_sm_set_enabled(i2s_config->pio, i2s_config->sm, false);

    /* Allocate memory for the DMA buffer */
    i2s_config->dma_buf = (uint16_t*) malloc(i2s_config->dma_trans_count*sizeof(uint32_t));

    /* Direct Memory Access setup */
    i2s_config->dma_channel = dma_claim_unused_channel(true);
    
    dma_channel_config dma_config = dma_channel_get_default_config(i2s_config->dma_channel);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(i2s_config->pio, i2s_config->sm, true));
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);

    channel_config_set_high_priority(&dma_config, true);
    channel_config_set_sniff_enable(&dma_config, false); // 禁用嗅探

    dma_channel_configure(i2s_config->dma_channel,
                          &dma_config,
                          &(i2s_config->pio->txf[i2s_config->sm]),    // Destination pointer
                          i2s_config->dma_buf,                        // Source pointer
                          i2s_config->dma_trans_count,                // Number of 32 bits words to transfer
                          false                                       // Start immediately
    );

    pio_sm_set_enabled(i2s_config->pio, i2s_config->sm , true);
}

/**
 * Write samples to I2S directly and wait for completion (blocking)
 * i2s_config: I2S context obtained by i2s_get_default_config()
 *     sample: pointer to an array of len x 32 bits samples
 *             Each 32 bits sample contains 2x16 bits samples, 
 *             one for the left channel and one for the right channel
 *        len: length of sample in 32 bits words
 */
void i2s_write(const i2s_config_t *i2s_config,const int16_t *samples,const size_t len) {
    for(size_t i=0;i<len;i++) {
            pio_sm_put_blocking(i2s_config->pio, i2s_config->sm, (uint32_t)samples[i]);
    }
}

/**
 * Write samples to DMA buffer and initiate DMA transfer (non blocking)
 * i2s_config: I2S context obtained by i2s_get_default_config()
 *     sample: pointer to an array of dma_trans_count x 32 bits samples
 */
void i2s_dma_write(i2s_config_t *i2s_config,const int16_t *samples) {
    /* Wait the completion of the previous DMA transfer */
    dma_channel_wait_for_finish_blocking(i2s_config->dma_channel);
    /* Copy samples into the DMA buffer */
    if(i2s_config->volume==0) {
        memcpy(i2s_config->dma_buf,samples,i2s_config->dma_trans_count*sizeof(int32_t));
    } else {
        for(uint16_t i=0;i<i2s_config->dma_trans_count*2;i++) {
            i2s_config->dma_buf[i] = samples[i]>>i2s_config->volume;
        }
    }
    
    /* Initiate the DMA transfer */
    dma_channel_transfer_from_buffer_now(i2s_config->dma_channel,
                                         i2s_config->dma_buf,
                                         i2s_config->dma_trans_count);
}

#if 0
/**
 * Adjust the output volume
 * i2s_config: I2S context obtained by i2s_get_default_config()
 *     volume: desired volume between 0 (highest. volume) and 16 (lowest volume)
 */
void i2s_volume(i2s_config_t *i2s_config,uint8_t volume) {
    if(volume>16) volume=16;
    i2s_config->volume=volume;
}

/**
 * Increases the output volume
 */
void i2s_increase_volume(i2s_config_t *i2s_config) {
    if(i2s_config->volume>0) {
        i2s_config->volume--;
    }
}

/**
 * Decreases the output volume
 */
void i2s_decrease_volume(i2s_config_t *i2s_config) {
    if(i2s_config->volume<16) {
        i2s_config->volume++;
    }
}
#endif