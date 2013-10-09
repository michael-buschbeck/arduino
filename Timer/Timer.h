///////////////////////////////////////////////////////////////////////////////
//
//  Non-blocking timers for Arduino projects.
//
//  (C) 2013 Michael Buschbeck <michael@buschbeck.net>
//
//  This work is licensed under a Creative Commons 3.0 Unported License and
//  distributed in the hope that it will be useful, but without any warranty.
//
//  See <http://creativecommons.org/licenses/by/3.0/> for details.
//


#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>


////////////////////////////////////////////////////////////////////////////////
//
//  Timer
//


class Timer
{
public:
  enum Schedule {
    STARTED   = 0x01,  STOPPED = 0x00,
    IMMEDIATE = 0x02,  DELAYED = 0x00,
    REPEAT    = 0x04,  ONCE    = 0x00,
  };

  Timer();
  Timer(uint8_t schedule);
  Timer(uint8_t schedule, unsigned long msecDelta);

  void start();
  void stop();
  bool due();

  bool active() const;
  bool repeat() const;

  void set(unsigned long msecDelta);
  void set(uint8_t schedule, unsigned long msecDelta);

protected:
  uint8_t _schedule;

  unsigned long _msecDelta;
  unsigned long _msecPrev;
};


////////////////////////////////////////////////////////////////////////////////
//
//  Timer (implementation)
//

inline Timer::Timer()
  : _schedule  (STOPPED | DELAYED | ONCE)
  , _msecDelta (0)
  , _msecPrev  (0)
{
  // nothing else to do
}


inline Timer::Timer(uint8_t schedule)
  : _schedule  (schedule)
  , _msecDelta (0)
  , _msecPrev  (0)
{
  if (active())
    start();
}


inline Timer::Timer(uint8_t schedule, unsigned long msecDelta)
  : _schedule  (schedule)
  , _msecDelta (msecDelta)
  , _msecPrev  (0)
{
  if (active())
    start();
}


inline void Timer::start()
{
  _msecPrev = millis();

  if (_schedule & IMMEDIATE)
    _msecPrev -= _msecDelta;

  _schedule |= STARTED;
}


inline void Timer::stop()
{
  _schedule &= ~STARTED;
}


inline bool Timer::due()
{
  if (!active())
    return false;

  if (millis() - _msecPrev < _msecDelta) {
    return false;
  }
  else {
    _msecPrev += _msecDelta;

    if (!repeat())
      stop();

    return true;
  }
}


inline bool Timer::active() const
{
  return (_schedule & STARTED);
}


inline bool Timer::repeat() const
{
  return (_schedule & REPEAT);
}


inline void Timer::set(unsigned long msecDelta)
{
  _msecDelta = msecDelta;
}


inline void Timer::set(uint8_t schedule, unsigned long msecDelta)
{
  bool activePrev = active();

  _schedule = schedule;
  _msecDelta = msecDelta;

  if (active() != activePrev) {
    if (active())
      start();
    else
      stop();
  }
}


#endif
