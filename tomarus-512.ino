#include "Controller.h"
#include "MidiController.h"

Controller controller;
MidiController midiController;

// for 74hc595
const int data = 2;
const int clock = 4;
const int latch = 3;

// "ScreenSaver" loop delay in ms
const int ssDelay = 100;

unsigned long ssNextLoop;
unsigned long ssNextLoop2;
int ssi = -1;
int ssj = 0;
int dir = 0;

// Rows LEDs
int activeRow = 1;
int activeCol = 1;
byte rows[5][3];
byte rowsold[5][3];
bool beat_led_on = false;

// Player

struct Col {
  byte velocity;
};

struct Row {
  byte note;
  byte length;
  byte speed;
  byte pos;
  byte tick; // internal
  Col cols[12];
};

// Default/Init pattern
Row Rows[5] = {
  { 36, 12, 6,  0, 0, { 100, 0, 0, 0, 100, 0, 0, 0,   100, 0, 0, 0 } }, // bd
  { 38, 8,  12, 0, 0, { 0, 0, 0, 0,   100, 0, 0, 100, 0, 0, 0, 0   } }, // snare
  { 42, 4,  3,  0, 0, { 100, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0   } }, // closed hh
  { 46, 12, 6,  0, 0, { 0, 0, 0, 0,   0, 0, 100, 0,   0, 0, 100, 0 } }, // open hh
  { 39, 10, 6,  0, 0, { 0, 0, 0, 0,   100, 0, 0, 0,   0, 0, 0, 0   } } // clap
};

void rowLoop(Row *row, int num) {
  row->tick++;
  if (row->tick >= row->speed) {
    row->tick = 0;

    setRowCol(num, row->pos, row->cols[row->pos].velocity > 0 ? true : false);
    row->pos++;
    if (row->pos >= row->length) {
      row->pos = 0;
    }
    setRowCol(num, row->pos, row->cols[row->pos].velocity > 0 ? false : true);

    if (row->cols[row->pos].velocity > 0) {
      midiController.Write(0x99, row->note, row->cols[row->pos].velocity);
    }
  }
}

void playerLoop() {
  for (int i = 0; i < 5; i++) {
    rowLoop(&Rows[i], i);
  }
}

void toggle() {
  Rows[activeRow].cols[activeCol].velocity = Rows[activeRow].cols[activeCol].velocity > 0 ? 0 : 100;
  setRowCol(activeRow, activeCol, Rows[activeRow].cols[activeCol].velocity > 0 ? true : false);
}

//

void setup() {
  pinMode(data, OUTPUT);
  pinMode(clock, OUTPUT);
  pinMode(latch, OUTPUT);

  controller.setup();
  controller.lcd()->setCursor(0, 0);
  controller.lcd()->print("TOMARUS-512");
  controller.keyCallback(keyCallback);
  controller.offCallback(offCallback);

  midiController.begin();
  midiController.TimerCallback(timerCallback);

  // set row 0 active
  activeRow = 0;
  //  setRowCol(0, 13, true);
}

void loop() {
  midiController.loop();
  controller.loop();
  matrixLoop();
  //  screenSaverLoop();
}

//

// Called on midi clock.
void timerCallback() {
  // Keep track of midi tick for beat led.
  static int tick = 0;

  beat_led_on = false;

  tick++;
  if (tick % 24 == 0) {
    beat_led_on = true;
  }

  playerLoop();
}

// Called when key has been released (keyUp handler).
void offCallback() {
  controller.lcd()->setCursor(14, 0);
  controller.lcd()->print("  ");
}

void keyCallback(int key) {
  setRowCol(activeRow, 13, false);

  switch (key) {
    case 1:
      activeRow = 0;
      break;
    case 0:
      activeRow = 1;
      break;
    case 12:
      activeRow = 2;
      break;
    case 13:
      activeRow = 3;
      break;
    case 14:
      activeRow = 4;
      break;
    case 17:
      activeCol = 0;
      toggle();
      break;
    case 18:
      activeCol = 1;
      toggle();
      break;
    case 15:
      activeCol = 2;
      toggle();
      break;
    case 16:
      activeCol = 3;
      toggle();
      break;
    case 2:
      activeCol = 4;
      toggle();
      break;
    case 3:
      activeCol = 5;
      toggle();
      break;
    case 21:
      activeCol = 6;
      toggle();
      break;
    case 20:
      activeCol = 7;
      toggle();
      break;
    case 8:
      activeCol = 8;
      toggle();
      break;
    case 9:
      activeCol = 9;
      toggle();
      break;
    case 10:
      activeCol = 10;
      toggle();
      break;
    case 11:
      activeCol = 11;
      toggle();
      break;
  }

  setRowCol(activeRow, 13, true);

  controller.lcd()->setCursor(14, 0);
  if (key < 10) controller.lcd()->print(" ");
  controller.lcd()->print(key);
}

void setRowCol(int row, int col, bool val) {
  byte bit2 = col == 14 ? 2 : 0;
  byte bit1 = col > 6 && col < 15 ? 2 << col - 7 : 0;
  byte bit0 = col <= 6 ? 2 << col : 0;

  if (val) {
    rows[row][2] |= bit2;
    rows[row][1] |= bit1;
    rows[row][0] |= bit0;
  } else {
    rows[row][2] &= ~bit2;
    rows[row][1] &= ~bit1;
    rows[row][0] &= ~bit0;
  }
}

void matrixLoop() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(latch, LOW);
    shiftOut(data, clock, 0);
    shiftOut(data, clock, 0);
    shiftOut(data, clock, 0);
    digitalWrite(latch, HIGH);
    delayMicroseconds(10);

    digitalWrite(latch, LOW);
    byte bit2 = rows[i][2] | 4 << (5 - i);
    shiftOut(data, clock, bit2 | (beat_led_on ? 4 : 0));
    shiftOut(data, clock, rows[i][1]);
    shiftOut(data, clock, rows[i][0]);
    digitalWrite(latch, HIGH);
    delayMicroseconds(100);
  }
}

// "ScreenSaver"
int randomatrix[60];

void screenSaverLoop() {
  unsigned long now = millis();

  // col loop
  if (now >= ssNextLoop) {
    ssNextLoop += ssDelay;

    switch (dir) {
      case 0:
        ssi++;
        if (ssi >= 24) {
          ssi = -1;
          dir = 1;
        }
        else if (ssi >= 12) {
          for (int j = 0; j < 5; j++) {
            setRowCol(j, ssi - 12, false);
          }
        } else {
          for (int j = 0; j < 5; j++) {
            setRowCol(j, ssi, true);
          }
        }
        break;

      case 1:
        ssi++;
        if (ssi >= 10) {
          ssi = -1;
          dir = 2;
        }
        else if (ssi >= 5) {
          for (int j = 0; j < 12; j++) {
            setRowCol(ssi - 5, j, false);
          }
        } else {
          for (int j = 0; j < 12; j++) {
            setRowCol(ssi, j, true);
          }
        }
        break;

      case 2:
        if (ssi == -1) {
          for (int j = 0; j < 12; j++) {
            setRowCol(0, j, true);
            setRowCol(1, j, true);
            setRowCol(2, j, true);
            setRowCol(3, j, true);
            setRowCol(4, j, true);
          }
          for (int j = 0; j < 60; j++) {
            randomatrix[j] = j;
          }
          for (int j = 0; j < 60; j++) {
            int r = random(0, 59);
            int x = randomatrix[j];
            randomatrix[j] = randomatrix[r];
            randomatrix[r] = x;
          }
          ssi = 0;
        } else {
          int r = randomatrix[ssi];
          setRowCol(r / 12, r % 12, false);
          r = randomatrix[ssi + 1];
          setRowCol(r / 12, r % 12, false);

          ssi += 2;
          if (ssi > 60) {
            dir = 0;
            ssi = -1;
          }
        }
        break;
    }
  }

  // row loop
  if (now >= ssNextLoop2) {
    ssNextLoop2 += ssDelay * 5;

    setRowCol(ssj, 13, false);
    ssj++;
    if (ssj > 4) ssj = 0;
    setRowCol(ssj, 13, true);
  }
}

void screenSaverLoop2() {
  unsigned long now = millis();
  if (now >= ssNextLoop) {
    ssNextLoop += ssDelay;

    setRowCol(ssj, ssi, false);
    ssi++;
    if (ssi >= 14) {
      ssi = 0;
      ssj++;
      if (ssj >= 5) ssj = 0;
    }
    setRowCol(ssj, ssi, true);
  }
}

// 74hc595 shift
void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
  for (int i = 7; i >= 0; i--)  {
    digitalWrite(myClockPin, 0);

    int pinState;
    if ( myDataOut & (1 << i) ) {
      pinState = 1;
    } else {
      pinState = 0;
    }

    digitalWrite(myDataPin, pinState);
    digitalWrite(myClockPin, 1);
    digitalWrite(myDataPin, 0);
  }
  digitalWrite(myClockPin, 0);
}

