// Segway Ninebot Zing C20 Legalizer Software for the Netherlands.
// It uses a PIR detector to detect the kick movement of the legs.


constexpr bool debug {true};
//constexpr bool debug {false};

// The timer library.
#include <millisDelay.h>
// The library for I2C
#include <Wire.h>
// The MCP 4725 DAC library.
#include <Adafruit_MCP4725.h>

bool flash_led_on {false};
millisDelay flash_led_delay {};

millisDelay sensor_timer {};

constexpr int throttle_input_pin {A0};
int throttle_input_value {0};

// The digital to analog converter.
Adafruit_MCP4725 digital_analog_converter;
int throttle_output_value {0};

constexpr int beeper_pin {8};
millisDelay beeper_timer {};

millisDelay motor_timer {};

// The motor will remain on for n seconds after the last leg movement detection or other trigger.
constexpr int motor_remain_on_seconds {8};

// The number of seconds that the motor-on is now timing out.
int motor_timeout_seconds_counter {0};

// The setup function runs once when you press reset or power the board.
void setup () 
{
  // Initialize the serial port for monitoring in debugging mode.
  if (debug) {
    Serial.begin (115200);
    Serial.println ();
    Serial.println ("Segway Ninebot Zing C20 Legalizing Software for the Netherlands");
  }

  // Initialize digital pin LED_BUILTIN as an output.
  // Most Arduinos have an on-board LED you can control. 
  // On the UNO, MEGA and ZERO it is attached to digital pin 13, on MKR1000 on pin 6. 
  // LED_BUILTIN is set to the correct LED pin independent of which board is used.
  // If you want to know what pin the on-board LED is connected to on your Arduino model, 
  // check the Technical Specs of your board at:
  // https://www.arduino.cc/en/Main/Products
  pinMode (LED_BUILTIN, OUTPUT);

  // Initialize the throttle input pin.
  pinMode (throttle_input_pin, INPUT);

  // The beeper pin.
  pinMode (beeper_pin, OUTPUT);

  // Begin the I2C communication at these pins:
  // Arduino A4 = SDA (serial data).
  // Arduino A5 = SCL (serial clock).
  Wire.begin();

  // For Adafruit MCP4725A1 the I2C address is 0x62 (default) or 0x63 (ADDR pin tied to VCC).
  digital_analog_converter.begin (0x62);
  
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
    
    // Optionally read the analog input from the photodiode and write it to the serial terminal.
    if (debug) {
      Serial.print (" throttle input ");
      Serial.print (throttle_input_value);
      Serial.print (" output ");
      Serial.print (throttle_output_value);
      Serial.print (" timer ");
      Serial.print (motor_timeout_seconds_counter);
      Serial.println ("");
    }
  }

  // Handle the timer to read the sensors.
  if (sensor_timer.justFinished()) {
    sensor_timer.repeat();

//    bool right_sensor_on = (right_photo_diode_value < previous_right_photo_diode_value - photo_diodes_sensitivity);
//    previous_right_photo_diode_value = right_photo_diode_value;
//    if (right_sensor_on) {
//      beep (1);
//      kick_motor_timer (motor_remain_on_seconds);
//    }

    // The analogRead() value is 10 bits wide so goes from 0 to 1023.
    throttle_input_value = analogRead (throttle_input_pin);

    // The DAC is 12 bits, to its input value can vary between 0 and 4095.
    // So multiply the input value by 4 then.
    int throttle_value_12_bits = 4 * throttle_input_value;

    if (throttle_value_12_bits != throttle_output_value) {
      // Normally the value to change the throttle output with is 10% of the absolute difference.
      // But if the difference is larger, then the throttle output immediately jumps to the final value.
      int difference = abs (throttle_value_12_bits - throttle_output_value);
      int change = difference / 10;
      if (difference > 1000) change = difference;
      // Change the throttle output up or down.
      if (throttle_value_12_bits < throttle_output_value) throttle_output_value -= change;
      if (throttle_value_12_bits > throttle_output_value) throttle_output_value += change;
      digital_analog_converter.setVoltage (throttle_output_value, false);
    }

    // If the throttle is released, the motor time-out restarts.
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
//      digitalWrite (throttle_relay_pin, HIGH);
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
//    digitalWrite (throttle_relay_pin, LOW);
  }
}
