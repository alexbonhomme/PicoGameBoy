#include "soundservice.h"

#if ENABLE_SOUND
/**
 * Global variables for audio task
 * stream contains N=AUDIO_SAMPLES samples
 * each sample is 32 bits
 * 16 bits for the left channel + 16 bits for the right channel in stereo interleaved format)
 * This is intended to be played at AUDIO_SAMPLE_RATE Hz
 */
uint16_t* stream;
i2s_config_t i2s_config;
SoundService::SoundService() { // 使用 memset 确保所有成员都被清零，避免未定义行为
  memset(&i2s_config, 0, sizeof(i2s_config_t));
}

void SoundService::initSound() {
  Serial.println("Initialize Sound ...");

  // Allocate memory for the stream buffer
  stream = (uint16_t*)malloc(AUDIO_BUFFER_SIZE_BYTES);
  assert(stream != NULL);
  memset(stream, 0, AUDIO_BUFFER_SIZE_BYTES); // Zero out the stream buffer

  // Initialize I2S sound driver
  i2s_config = i2s_get_default_config();
  i2s_config.sample_freq = AUDIO_SAMPLE_RATE;
  i2s_config.dma_trans_count = AUDIO_SAMPLES;
  i2s_config.data_pin = I2S_DIN_PIN,
  i2s_config.clock_pin_base = I2S_BCLK_LRC_PIN_BASE;
  // 尝试使用PIO1，如果失败则使用PIO2
  i2s_config.pio = pio1; // 使用PIO1专门处理I2S，避免与TFT_eSPI的PIO0冲突
  i2s_init(&i2s_config);

  Serial.println("Sound initialized");
}

void SoundService::handleSoundLoop() {
  if (i2s_config.volume != 16) {
    if(_audioCallback){
      _audioCallback(NULL, (int16_t*)stream, AUDIO_BUFFER_SIZE_BYTES);
      i2s_dma_write(&i2s_config, (int16_t*)stream);
    }
  }
}

uint8_t SoundService::getVolume() {
  return i2s_config.volume;
}

void SoundService::increaseVolume() {
  if (i2s_config.volume > 0) {
    i2s_config.volume--;
  }
}

void SoundService::decreaseVolume() {
  if (i2s_config.volume < 16) {
    i2s_config.volume++;
  }
}

void SoundService::setAudioCallback(std::function<void(void *userdata, int16_t *stream, size_t len)> audioCallback) {
  _audioCallback = audioCallback;
  Serial.println("I Sound callback set.");
}

#endif