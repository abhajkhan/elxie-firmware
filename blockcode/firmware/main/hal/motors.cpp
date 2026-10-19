#include "motors.h"

#include <math.h>

#include <initializer_list>

#include "board_config.h"
#include "platform.h"

#if __has_include("soc/usb_serial_jtag_reg.h")
#include "soc/soc.h"
#include "soc/usb_serial_jtag_reg.h"
#define ELX_HAS_USB_JTAG 1
#endif

// TB6612FNG: speed on PWMx, direction on xIN1/xIN2, STBY high = enabled.
//   IN1=1 IN2=0 forward · IN1=0 IN2=1 reverse · IN1=IN2=1 short brake · PWM=0 coast
namespace motors {

static const uint32_t kMaxDuty = (1u << MOTOR_PWM_BITS) - 1;

struct Motor { int pwm, in1, in2, ch; bool invert; };
static const Motor kL = {PIN_MOTOR_L_PWM, PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, LEDC_CH_MOTOR_L, MOTOR_L_INVERT};
static const Motor kR = {PIN_MOTOR_R_PWM, PIN_MOTOR_R_IN1, PIN_MOTOR_R_IN2, LEDC_CH_MOTOR_R, MOTOR_R_INVERT};

static void drive(const Motor& m, float pct) {
  if (m.invert) pct = -pct;
  float mag = fabsf(pct);
  if (mag < 1.0f) {  // coast
    hal::pwmWrite(m.ch, 0);
    return;
  }
  if (mag > 100.0f) mag = 100.0f;
  // Map 1..100 % onto MIN_DUTY..1.0 so low speeds still move the wheels.
  float frac = MOTOR_MIN_DUTY + (1.0f - MOTOR_MIN_DUTY) * (mag / 100.0f);
  hal::pinWrite(m.in1, pct > 0 ? 1 : 0);
  hal::pinWrite(m.in2, pct > 0 ? 0 : 1);
  hal::pwmWrite(m.ch, (uint32_t)(frac * kMaxDuty));
}

static void brakeOne(const Motor& m) {
  hal::pinWrite(m.in1, 1);
  hal::pinWrite(m.in2, 1);
  hal::pwmWrite(m.ch, kMaxDuty);
}

// GPIO19/20 are the ESP32-S3's native USB D-/D+. The USB-Serial-JTAG block keeps
// its pad and D+ pull-up attached after boot, which fights the motor driver inputs.
// Elxie talks over UART0 (USB-UART chip), so detach the native USB pads.
static void releaseUsbPins() {
#ifdef ELX_HAS_USB_JTAG
  SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP | USB_SERIAL_JTAG_DM_PULLUP);
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
#endif
}

void begin() {
  releaseUsbPins();
  hal::pinOutput(PIN_MOTOR_STBY, 0);  // keep the driver off while configuring
  for (const Motor* m : {&kL, &kR}) {
    hal::pinOutput(m->in1, 0);
    hal::pinOutput(m->in2, 0);
  }
  hal::pwmTimer(LEDC_TIMER_MOTOR, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
  hal::pwmAttach(kL.pwm, kL.ch, LEDC_TIMER_MOTOR);
  hal::pwmAttach(kR.pwm, kR.ch, LEDC_TIMER_MOTOR);
  hal::pinWrite(PIN_MOTOR_STBY, 1);
}

void write(float leftPct, float rightPct) {
  drive(kL, leftPct);
  drive(kR, rightPct);
}

void brake() {
  brakeOne(kL);
  brakeOne(kR);
}

}  // namespace motors
