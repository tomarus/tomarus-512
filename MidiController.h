#ifndef MIDICONTROLLER_H
#define MIDICONTROLLER_H

#include "Arduino.h"

class MidiController
{
  public:
    MidiController();
    void begin();
    void loop();
    void Write(byte b);
    void Write(byte b1, byte b2, byte b3);
    void Start();
    void Stop(bool hard);
    void Clock();
    void TimerCallback(void (*)());
    void SetBPM(float);
    void ResetTimer();
  private:
    void setupTimer();

    void (*_timerCallback)();
    unsigned long _sleep;
    unsigned long _next;
};

#endif

