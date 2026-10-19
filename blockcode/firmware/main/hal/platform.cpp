#include "platform.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char* TAG = "hal";

namespace hal {

SemaphoreHandle_t i2cMutex = nullptr;
static i2c_master_bus_handle_t s_bus = nullptr;

void initI2c() {
  i2cMutex = xSemaphoreCreateMutex();
  i2c_master_bus_config_t cfg = {};
  cfg.i2c_port = I2C_NUM_0;
  cfg.sda_io_num = (gpio_num_t)PIN_I2C_SDA;
  cfg.scl_io_num = (gpio_num_t)PIN_I2C_SCL;
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  cfg.flags.enable_internal_pullup = 1;  // add external 4.7k pull-ups for 400 kHz
  ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));
}

i2c_master_dev_handle_t addI2cDevice(uint8_t addr) {
  i2c_device_config_t dev = {};
  dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev.device_address = addr;
  dev.scl_speed_hz = I2C_FREQ_HZ;
  i2c_master_dev_handle_t h = nullptr;
  if (i2c_master_bus_add_device(s_bus, &dev, &h) != ESP_OK) ESP_LOGE(TAG, "i2c add 0x%02x failed", addr);
  return h;
}

bool i2cProbe(uint8_t addr) {
  I2cLock lock(50);
  return lock.ok && i2c_master_probe(s_bus, addr, 20) == ESP_OK;
}

bool i2cWriteReg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
  if (!dev) return false;
  uint8_t b[2] = {reg, val};
  return i2c_master_transmit(dev, b, 2, 20) == ESP_OK;
}

bool i2cReadRegs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* out, size_t n, int timeoutMs) {
  if (!dev) return false;
  return i2c_master_transmit_receive(dev, &reg, 1, out, n, timeoutMs) == ESP_OK;
}

// ---------------------------------------------------------------- ADC
static adc_oneshot_unit_handle_t s_adc1 = nullptr;
struct AdcPin {
  int gpio;
  adc_channel_t ch;
};
static AdcPin s_adcPins[8];
static int s_adcCount = 0;

bool adcAttach(int gpio) {
  adc_unit_t unit;
  adc_channel_t ch;
  if (adc_oneshot_io_to_channel(gpio, &unit, &ch) != ESP_OK || unit != ADC_UNIT_1) {
    ESP_LOGE(TAG, "GPIO %d is not an ADC1 pin", gpio);
    return false;
  }
  if (!s_adc1) {
    adc_oneshot_unit_init_cfg_t ucfg = {};
    ucfg.unit_id = ADC_UNIT_1;
    ucfg.ulp_mode = ADC_ULP_MODE_DISABLE;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &s_adc1));
  }
  adc_oneshot_chan_cfg_t ccfg = {};
  ccfg.atten = ADC_ATTEN_DB_12;  // full 0..3.1 V range
  ccfg.bitwidth = ADC_BITWIDTH_12;
  ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, ch, &ccfg));
  if (s_adcCount < 8) s_adcPins[s_adcCount++] = {gpio, ch};
  return true;
}

int adcRead(int gpio) {
  for (int i = 0; i < s_adcCount; ++i) {
    if (s_adcPins[i].gpio == gpio) {
      int raw = 0;
      adc_oneshot_read(s_adc1, s_adcPins[i].ch, &raw);
      return raw;
    }
  }
  return 0;
}

// ---------------------------------------------------------------- LEDC
void pwmTimer(int timer, uint32_t freqHz, int bits) {
  ledc_timer_config_t t = {};
  t.speed_mode = LEDC_LOW_SPEED_MODE;
  t.duty_resolution = (ledc_timer_bit_t)bits;
  t.timer_num = (ledc_timer_t)timer;
  t.freq_hz = freqHz;
  t.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&t));
}

void pwmAttach(int gpio, int channel, int timer) {
  ledc_channel_config_t c = {};
  c.gpio_num = gpio;
  c.speed_mode = LEDC_LOW_SPEED_MODE;
  c.channel = (ledc_channel_t)channel;
  c.intr_type = LEDC_INTR_DISABLE;
  c.timer_sel = (ledc_timer_t)timer;
  c.duty = 0;
  c.hpoint = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&c));
}

void pwmWrite(int channel, uint32_t duty) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

void pwmFreq(int timer, uint32_t freqHz) { ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer, freqHz); }

// ---------------------------------------------------------------- GPIO
void pinOutput(int gpio, int level) {
  gpio_config_t c = {};
  c.pin_bit_mask = 1ULL << gpio;
  c.mode = GPIO_MODE_OUTPUT;
  gpio_config(&c);
  gpio_set_level((gpio_num_t)gpio, level);
}

void pinInput(int gpio, bool pullUp, bool pullDown) {
  gpio_config_t c = {};
  c.pin_bit_mask = 1ULL << gpio;
  c.mode = GPIO_MODE_INPUT;
  c.pull_up_en = pullUp ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  c.pull_down_en = pullDown ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
  gpio_config(&c);
}

void pinWrite(int gpio, int level) { gpio_set_level((gpio_num_t)gpio, level); }
int pinRead(int gpio) { return gpio_get_level((gpio_num_t)gpio); }

}  // namespace hal
