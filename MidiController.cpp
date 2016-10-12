#include "Arduino.h"
#include "MidiController.h"
//#include <SoftwareSerial.h>
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

float bpm = 125;
MidiController *instance;

//

struct mySettings : public midi::DefaultSettings {
  static const bool UseRunningStatus = false;
  static const bool Use1ByteParsing = true;
  static const bool HandleNullVelocityNoteOnAsNoteOff = false;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial, midiIN, mySettings);

static void outByte(byte b) {
  Serial.write(b);
}

static void outBytes(byte b1, byte b2, byte b3) {
  outByte(b1);
  outByte(b2);
  outByte(b3);
}

static void handleAllMessages(midi::MidiMessage msg) {
  if (msg.type == midi::Start) {
    outByte(msg.type);
    instance->ResetTimer();
    return;
  }
  if (msg.type == midi::Stop) {
    outByte(msg.type);
    return;
  }

  if (msg.type < 0xf0) {
    outByte(msg.type + msg.channel - 1);
    outByte(msg.data1);
    if (msg.type <= 0xf0 || msg.data2) {
      outByte(msg.data2);
    }
  }
}

static void handleSysex(byte *array, unsigned size) {
  if (size < 5) return;
  if (!(array[1] == 0x7d && array[2] == 0x2a && array[3] == 0x4d)) return;

  for (int i = 4; i < size - 1; i++) {
    int command = array[i];
    if (command >= 0x40) {
      return;
    }

    switch (command) {
      case 0: {
          byte response[] = {0xf0, 0x7d, 0x2a, 0x4d,
                             0x40, 0x01,            // RESPONSE 0x40, version 1
                             0x01, 0x02,            // 1 inport, 2 outports
                             byte(int(bpm) >> 7),   // current speet msb
                             byte(int(bpm) & 0x7f), // current speed lsb
                             0xf7
                            };
          midiIN.sendSysEx(sizeof(response), response, true);
          return;
        }
      case 1: {
          i += 7;
          break;
        }
      case 2: {
          int newbpm = (array[i + 1] << 7) + array[i + 2];
          instance->SetBPM(newbpm);
          break;
        }
      case 3: {
          instance->Start();
          break;
        }
      case 4: {
          instance->Stop(false);
          break;
        }
      default: {
          break;
        }
    }
  }
}

//

MidiController::MidiController() {}

void MidiController::begin() {
  instance = this;

  midiIN.begin(MIDI_CHANNEL_OMNI);
  midiIN.turnThruOff();
  midiIN.setHandleSystemExclusive(handleSysex);
  midiIN.setHandleAllMessages(handleAllMessages);

  this->setupTimer();
}

void MidiController::loop() {
  unsigned long now = micros();
  if (now >= _next) {
    _next += _sleep;

    Clock();
    if (_timerCallback) {
      _timerCallback();
    }
  }
  midiIN.read();
}

void MidiController::Write(byte b) {
  outByte(b);
}

void MidiController::Write(byte b1, byte b2, byte b3) {
  outByte(b1);
  outByte(b2);
  outByte(b3);
}

void MidiController::Start() {
  midiIN.sendRealTime(midi::Start);

  this->ResetTimer();
}

void MidiController::Stop(bool hard) {
  midiIN.sendRealTime(midi::Stop);

  //  if (hard) {
  //    Timer1.stop();
  //  }
}

byte lfo = 0;
bool lfodir = false;
int lfochan = 6;
byte lfomsb = 0x01;
byte lfolsb = 0x20;

void MidiController::Clock() {
  midiIN.sendRealTime(midi::Clock);

  if (lfodir) {
    lfo--;
    if (lfo == 0) {
      lfodir = false;
    }
  } else {
    lfo++;
    if (lfo > 0x7f) {
      lfodir = true;
    }
  }
  outBytes(0xb0 + lfochan - 1, 0x63, lfomsb);
  outBytes(0xb0 + lfochan - 1, 0x62, lfolsb);
  outBytes(0xb0 + lfochan - 1, 0x06, lfo);
  outBytes(0xb0 + lfochan - 1, 0x26, 0);

  //  outBytes(0xb5, 0x65, lfo);
  //  outBytes(0xb5, 0x64, 0);
}

void MidiController::TimerCallback(void (*fn)(void)) {
  _timerCallback = fn;
}

void MidiController::SetBPM(float newbpm) {
  bpm = newbpm;
  setupTimer();
}

//

void MidiController::ResetTimer() {
  this->setupTimer();
  _next = micros() - 1000;
}

void MidiController::setupTimer() {
  _sleep = 60000000.0 / bpm / 24.0;
}

