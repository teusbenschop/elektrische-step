// Segway Ninebot Zing C20 Legalizer Software for the Netherlands.


constexpr bool debug {true};
//constexpr bool debug {false};

#include <millisDelay.h>

bool flash_led_on {false};
millisDelay flash_led_delay {};

millisDelay sensor_timer {};

constexpr int photo_diodes_sensitivity {15};

constexpr int photo_diodes_noise_reducing_read_count {10};

constexpr int left_photo_diode_pin {A0};
int left_photo_diode_value {0};
int previous_left_photo_diode_value {0};

constexpr int right_photo_diode_pin {A5};
int right_photo_diode_value {0};
int previous_right_photo_diode_value {0};

constexpr int throttle_input_pin {A2};
int throttle_input_value {0};
// The analogWrite (pin, val) function is applicable to PWM pins (D3, D5, D6, D9, D10, and D11 in Arduino Nano).
// But analogWrite is no longer used on this pin. The scooter motor was stuttering too much.
constexpr int throttle_relay_pin {A1};

// The pin and timer to control the beeper.
constexpr int beeper_pin {8};
millisDelay beeper_timer {};

// The timer to enable the motor.
millisDelay motor_timer {};
// The motor will remain on for n seconds after the last leg movement detection or other trigger.
constexpr int motor_remain_on_seconds {8};
// The number of seconds that the motor-on is timing out.
int motor_timeout_seconds_counter {0};

// The setup function runs once when you press reset or power the board.
void setup () 
{

  // Initialize the serial port for monitoring in debugging mode.
  if (debug) {
    Serial.begin (115200);
    Serial.println ();
    Serial.println ("Segway Ninebot Zing C20 Legalizing Software for the Netherlands version 003");
  }

  // Initialize digital pin LED_BUILTIN as an output.
  // Most Arduinos have an on-board LED you can control. 
  // On the UNO, MEGA and ZERO it is attached to digital pin 13, on MKR1000 on pin 6. 
  // LED_BUILTIN is set to the correct LED pin independent of which board is used.
  // If you want to know what pin the on-board LED is connected to on your Arduino model, 
  // check the Technical Specs of your board at:
  // https://www.arduino.cc/en/Main/Products
  pinMode (LED_BUILTIN, OUTPUT);

  // Initialize the ADC (analog-digital converter).
  // https://reference.arduino.cc/reference/en/language/functions/analog-io/analogreference/
  // https://forum.arduino.cc/t/arduino-nano-undocumented-feature-analogreference-0/362161
  // Be careful with this, as incorrect settings can damage the microcontroller.
  // If the analog reference is not set to "external" but an external voltage is applied to the "ref" pin,
  // this can damage the MCU.
  //analogReference (EXTERNAL);
  
  // Initialize the sensor input pins.
  pinMode (left_photo_diode_pin, INPUT);
  pinMode (right_photo_diode_pin, INPUT);

  // Initialize the throttle input pin.
  pinMode (throttle_input_pin, INPUT);
  // Set the throttle output PWM frequency as high as possible.
  // https://www.etechnophiles.com/how-to-change-the-pwm-frequency-of-arduino-nano/
  // TCCR1B = TCCR1B & B11111000 | B00000001;
  // Initialize the throttle output pin.
  pinMode (throttle_relay_pin, OUTPUT);
  // Initialize the output with some value to avoid errors and red flashing LED of the Ninebot controller,
  // as it initially detects too low an input Voltage on the throttle input.
  // analogWrite (throttle_output_pin, 50);

  // The beeper pin.
  pinMode (beeper_pin, OUTPUT);
  
  // Start the LED flash timer with nnn milliseconds.
  // This is the period between toggling the LED on or off.
  flash_led_delay.start(500);

  // Start the timer for reading the PIR motion sensor and the IR photo diodes every nn milliseconds.
  sensor_timer.start (50);

  // The motor timer will do a cycle every second for doing the housekeeping.
  motor_timer.start (1000);
}


// The loop function runs over and over again forever.
void loop () 
{

  // Handle LED flash timeout.
  if (flash_led_delay.justFinished()) {
    // Repeat the timer.
    flash_led_delay.repeat();
    // Toggle the LED-on flag.
    flash_led_on = !flash_led_on;
    // Turn the LED on or off.
    digitalWrite (LED_BUILTIN, flash_led_on ? HIGH : LOW);
    //digitalWrite (beeper_pin, flash_led_on ? HIGH : LOW);
    //digitalWrite (throttle_relay_pin, flash_led_on ? HIGH : LOW);
    
    // Optionally read the analog input from the photodiode and write it to the serial terminal.
    if (debug) {
      int value {0};
      Serial.print ("left ");
      Serial.print (left_photo_diode_value);
      Serial.print (" right ");
      Serial.print (right_photo_diode_value);
      Serial.print (" throttle ");
      Serial.print (throttle_input_value);
      Serial.print (" timer ");
      Serial.print (motor_timeout_seconds_counter);
      Serial.println ("");
    }
  }

  // Handle the timer to read the sensors.
  if (sensor_timer.justFinished()) {
    sensor_timer.repeat();

    // Read the analog input of the left and right photo diode multiple times to reduce noise.
    left_photo_diode_value = 0;
    right_photo_diode_value = 0;
    for (int i {0}; i < photo_diodes_noise_reducing_read_count; i++) {
      left_photo_diode_value += analogRead(left_photo_diode_pin);
      right_photo_diode_value += analogRead(right_photo_diode_pin);
    }
    left_photo_diode_value /= photo_diodes_noise_reducing_read_count;
    right_photo_diode_value /= photo_diodes_noise_reducing_read_count;

    bool left_sensor_on = (left_photo_diode_value < previous_left_photo_diode_value - photo_diodes_sensitivity);
    previous_left_photo_diode_value = left_photo_diode_value;
    if (left_sensor_on) {
      beep (1);
      kick_motor_timer (motor_remain_on_seconds);
    }

    bool right_sensor_on = (right_photo_diode_value < previous_right_photo_diode_value - photo_diodes_sensitivity);
    previous_right_photo_diode_value = right_photo_diode_value;
    if (right_sensor_on) {
      beep (1);
      kick_motor_timer (motor_remain_on_seconds);
    }

    throttle_input_value = analogRead (throttle_input_pin);
    // The analogRead() values go from 0 to 1023, analogWrite() values from 0 to 255, so divide by 4 (in theory).
    // There is a 2kΩ resistor from the PWM output, and a 100µF capacitor to the ground, 
    // and then this goes to the throttle input of the Ninebot controller.
    // The Ninebot controller draws a bit of current too.
    // So by trial and error, the throttle division factor is determined as below instead of being 4.
    //constexpr float throttle_division_factor {3.55};
    //analogWrite (throttle_output_pin, throttle_input_value / throttle_division_factor);
    // Just now do it such that if the throttle is closed three times, a new period of time is given to the motor enable.
    if (throttle_input_value < 200) {
      kick_motor_timer ((motor_remain_on_seconds / 3) + 1);
    }
  }  

  if (beeper_timer.justFinished()) {
    digitalWrite (beeper_pin, LOW);
  }

  if (motor_timer.justFinished ()) {
    // Repeat the timer.
    motor_timer.repeat();
    // Increase the timeout counter.
    motor_timeout_seconds_counter++;
    // Check if the motor should be switched off.
    if (motor_timeout_seconds_counter >= motor_remain_on_seconds) {
      digitalWrite (throttle_relay_pin, HIGH);
    }
    
  }

}

void beep (const int milliseconds)
{
  beeper_timer.start (milliseconds);
  digitalWrite (beeper_pin, HIGH);
}


void kick_motor_timer (int value)
{
  motor_timeout_seconds_counter -= value;
  if (motor_timeout_seconds_counter < 0) motor_timeout_seconds_counter = 0;
  if (motor_timeout_seconds_counter == 0) {
    digitalWrite (throttle_relay_pin, LOW);
  }
}
