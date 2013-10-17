///////////////////////////////////////////////////////////////////////////////
//
//  Non-blocking library for VS1053b-based Arduino expansion boards.
//
//  (C) 2013 Michael Buschbeck <michael@buschbeck.net>
//
//  Licensed under a Creative Commons Attribution 3.0 Unported License and
//  distributed in the hope that it will be useful, but without any warranty.
//
//  See <http://creativecommons.org/licenses/by/3.0/> for details.
//


#ifndef MUSIC_H
#define MUSIC_H

#include <avr/io.h>
#include <avr/pgmspace.h>

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "Pin.h"


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic
//

class CMusic
{
public:
  CMusic();

  void begin(
    uint8_t addressPinReset         = A0,
    uint8_t addressPinRequest       = A1,
    uint8_t addressPinSelectData    = A2,
    uint8_t addressPinSelectControl = A3);

  void reset(bool hardware = true, bool settings = true);

  enum State {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_BUSY,
  };

  State state();

  bool play(File& source);
  bool cancel();
  bool loop(unsigned long msecMax = 0);

  int time();

  void volume(uint8_t volume);
  uint8_t volume();

  void balance(int8_t balance);
  int8_t balance();

private:
  static uint8_t const SCI_OPCODE_WRITE;
  static uint8_t const SCI_OPCODE_READ;

  class Register;
  class Memory;

  template<class TReadable>
  typename TReadable::Value read();

  template<class TWritable>
  void write(typename TWritable::Value value);

  size_t sendAudio(size_t nBytesMax);
  size_t sendFlush(size_t nBytesMax);

  void updateVolumeAndBalance();

  
  PinDigital<OUTPUT> _pinReset;          // RESET
  PinDigital<INPUT>  _pinRequest;        // DREQ
  PinDigital<OUTPUT> _pinSelectData;     // SS_SDI
  PinDigital<OUTPUT> _pinSelectControl;  // SS_SCI


  template<size_t SIZE>
  class Buffer
  {
  public:
    Buffer();

    void open(File& file);
    void refill(size_t nBytesMin);
    void close();

    bool active();
    size_t available() const;

    unsigned char read();

  private:
    File* _file;
    unsigned char _buffer[SIZE];
    size_t _offset;
    size_t _length;
  };


  Buffer<64> _buffer;

  bool _cancel;

  enum ActionCancel {
    ACTION_CANCEL_NONE,
    ACTION_CANCEL_SET_IMMEDIATE,
    ACTION_CANCEL_SET_AFTER_FLUSH,
  };

  enum ActionBuffer {
    ACTION_BUFFER_NONE,
    ACTION_BUFFER_CLOSE_AFTER_CANCEL,
  };

  ActionCancel _actionCancel;
  ActionBuffer _actionBuffer;

  size_t _nBytesFlushRemaining;

  uint8_t _volume;
  int8_t _balance;

  unsigned long _msecPlaybackStart;
};


////////////////////////////////////////////////////////////////////////////////
//
//  interface
//

extern CMusic Music;

static CMusic::State const MUSIC_STATE_IDLE    = CMusic::STATE_IDLE;
static CMusic::State const MUSIC_STATE_PLAYING = CMusic::STATE_PLAYING;
static CMusic::State const MUSIC_STATE_BUSY    = CMusic::STATE_BUSY;


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic::Buffer (implementation)
//

template<size_t SIZE>
inline bool CMusic::Buffer<SIZE>::active()
{
  return (bool) _file;
}


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic (implementation)
//

inline CMusic::State CMusic::state()
{
  if (_cancel || _nBytesFlushRemaining > 0)
    return STATE_BUSY;

  if (_buffer.active())
         return STATE_PLAYING;
    else return STATE_IDLE;
}


inline int CMusic::time()
{
  if (state() != STATE_PLAYING)
    return 0;

  return (millis() - _msecPlaybackStart) / 1000;
}


inline void CMusic::volume(uint8_t volume)
{
  _volume = volume;
  updateVolumeAndBalance();
}


inline uint8_t CMusic::volume()
{
  return _volume;
}


inline void CMusic::balance(int8_t balance)
{
  _balance = balance;
  updateVolumeAndBalance();
}


inline int8_t CMusic::balance()
{
  return _balance;
}


#endif
