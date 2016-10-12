//
// Tomarus LCD + Controller lib
//
// References:
//   74HC165:
//     - http://playground.arduino.cc/Code/ShiftRegSN74HC165N
//   DFRobot I2C LCD
//     - https://github.com/openenergymonitor/LiquidCrystal_I2C
//   Rotary Encoder
//     - http://playground.arduino.cc/Main/RotaryEncoders
//

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int ploadPin       = 5;  // Connects to Parallel load pin the 165
const int clockEnablePin = 8;  // Connects to Clock Enable pin the 165 // XXX NOT USED
const int dataPin        = 7; // Connects to the Q7 pin the 165
const int clockPin       = 6; // Connects to the Clock pin the 165

const int pollDelayMsec  = 1;  // Poll Delay in Milliseconds

// Custom LCD Chars.
uint8_t scrollArrow[8] = {0x04, 0x0e, 0x1f, 0x00, 0x00, 0x1f, 0x0e, 0x04};
uint8_t scrollDown[8]  = {0x00, 0x00, 0x00, 0x00, 0x1f, 0x0e, 0x04, 0x00};
uint8_t scrollUp[8]    = {0x00, 0x04, 0x0e, 0x1f, 0x00, 0x00, 0x00, 0x00};
uint8_t blockSmall[8]  = {0x00, 0x00, 0x0e, 0x0e, 0x0e, 0x0e, 0x00, 0x00};
uint8_t scrollLeft[8]  = {0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01, 0x00};
uint8_t scrollRight[8] = {0x10, 0x18, 0x1c, 0x1e, 0x1c, 0x18, 0x10, 0x00};
uint8_t pauseButton[8] = {0x00, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x00};

class Menu {
  private:
    LiquidCrystal_I2C *m_lcd;

  public:
    void setup() {
      m_lcd = new LiquidCrystal_I2C(0x27, 16, 2);
      m_lcd->init();
      m_lcd->clear();
      m_lcd->createChar(0, scrollArrow);
      m_lcd->createChar(1, scrollUp);
      m_lcd->createChar(2, scrollDown);
      m_lcd->createChar(3, blockSmall);
      m_lcd->createChar(4, scrollLeft);
      m_lcd->createChar(5, scrollRight);
      m_lcd->createChar(6, pauseButton);
      m_lcd->backlight();
    }

    LiquidCrystal_I2C* lcd() {
      return m_lcd;
    }
};

// Shift Register Setup
#define NUMBER_OF_SHIFT_CHIPS  3
#define DATA_WIDTH             NUMBER_OF_SHIFT_CHIPS * 8
#define PULSE_WIDTH_USEC       5
#define BYTES_VAL_T            unsigned long

class Controller {
  private:

    Menu m_menu;
    unsigned long nextCheck;
    int encoder0PinALast = LOW;
    BYTES_VAL_T pinValues;
    BYTES_VAL_T oldPinValues;
    int horiz = 0;
    int vert = 0;
    void (*keycb)(int);
    void (*offcb)(void);

    // This function is essentially a "shift-in" routine reading the
    // serial Data from the shift register chips and representing
    // the state of those pins in an unsigned integer (or long).
    BYTES_VAL_T read_shift_regs() {
      long bitVal;
      BYTES_VAL_T bytesVal = 0;

      // Trigger a parallel Load to latch the state of the data lines,
      digitalWrite(clockEnablePin, HIGH);
      digitalWrite(ploadPin, LOW);
      delayMicroseconds(PULSE_WIDTH_USEC);
      digitalWrite(ploadPin, HIGH);
      digitalWrite(clockEnablePin, LOW);

      // Loop to read each bit value from the serial out line of the 74HC165.
      for (int i = 0; i < DATA_WIDTH; i++) {
        bitVal = digitalRead(dataPin);

        // Set the corresponding bit in bytesVal.
        bytesVal |= (bitVal << ((DATA_WIDTH - 1) - i));

        // Pulse the Clock (rising edge shifts the next bit).
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(PULSE_WIDTH_USEC);
        digitalWrite(clockPin, LOW);
      }

      return (bytesVal);
    }

    void check_pin_values() {
      if (pinValues == 0) {
        if (offcb != 0) offcb();
        return;
      }
      for (int i = 0; i < DATA_WIDTH; i++) {
        if (keycb != 0) keycb(i);
      }
    }

    void poller() {
      // Read 74HC165
      pinValues = read_shift_regs();
      if (pinValues != oldPinValues) {
        check_pin_values();
        oldPinValues = pinValues;
      }
    }

  public:

    void setup() {
      // Initialize 74HC165
      pinMode(ploadPin, OUTPUT);
      pinMode(clockEnablePin, OUTPUT);
      pinMode(clockPin, OUTPUT);
      pinMode(dataPin, INPUT);
      digitalWrite(clockPin, LOW);
      digitalWrite(ploadPin, HIGH);
      pinValues = read_shift_regs();
      oldPinValues = pinValues;

      // Initialize menu
      m_menu.setup();
    }

    void loop() {
      unsigned long now = micros();
      if (now >= nextCheck) {
        nextCheck += (pollDelayMsec * 1000);
        poller();
      }
    }

    void keyCallback(void (*f)(int)) {
      keycb = f;
    }

    void offCallback(void (*f)(void)) {
      offcb = f;
    }

    LiquidCrystal_I2C* lcd() {
      return m_menu.lcd();
    }
};

