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


#include "Music.h"


////////////////////////////////////////////////////////////////////////////////
//
//  Music (instantiation)
//

CMusic Music;


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic
//

uint8_t const CMusic::SCI_OPCODE_WRITE = 0x02;
uint8_t const CMusic::SCI_OPCODE_READ  = 0x03;


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic::Register
//

class CMusic::Register
{
private:
  template<uint8_t ADDRESS, class TWriteUntil>
  class Binding
  {
  public:
    typedef uint16_t Value;

    static uint16_t read(CMusic& music);
    static void write(CMusic& music, uint16_t value);
  };

  class WriteDisabled
  {
    // empty
  };

  class WriteUntilPin
  {
  public:
    static void wait(CMusic& music);
  };

  template<unsigned long TICKS>
  class WriteUntilPinOrTimeout
  {
  public:
    static void wait(CMusic& music);
  };

public:
  static uint16_t const SM_DIFF          = (0x1 <<  0);
  static uint16_t const SM_RESET         = (0x1 <<  2);
  static uint16_t const SM_CANCEL        = (0x1 <<  3);
  static uint16_t const SM_EARSPEAKER_LO = (0x1 <<  4);
  static uint16_t const SM_EARSPEAKER_HI = (0x1 <<  7);
  static uint16_t const SM_SDINEW        = (0x1 << 11);

  typedef Binding<0x0, WriteUntilPinOrTimeout< 80> > SCI_MODE;
  typedef Binding<0x1, WriteUntilPinOrTimeout< 80> > SCI_STATUS;
  typedef Binding<0x3, WriteUntilPin               > SCI_CLOCKF;
  typedef Binding<0x4, WriteUntilPinOrTimeout<100> > SCI_DECODE_TIME;
  typedef Binding<0x5, WriteUntilPin               > SCI_AUDATA;
  typedef Binding<0x6, WriteUntilPinOrTimeout<100> > SCI_WRAM;
  typedef Binding<0x7, WriteUntilPinOrTimeout<100> > SCI_WRAMADDR;
  typedef Binding<0x8, WriteDisabled               > SCI_HDAT0;
  typedef Binding<0x9, WriteDisabled               > SCI_HDAT1;
  typedef Binding<0xB, WriteUntilPinOrTimeout< 80> > SCI_VOL;
};


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic::Memory
//

class CMusic::Memory
{
private:
  template<uint16_t ADDRESS, typename TValue>
  class Binding
  {
    // empty base template
  };

  template<uint16_t ADDRESS>
  class Binding<ADDRESS, uint16_t>
  {
  public:
    typedef uint16_t Value;

    static uint16_t read(CMusic& music);
    static void write(CMusic& music, uint16_t value);
  };

  template<uint16_t ADDRESS>
  class Binding<ADDRESS, uint32_t>
  {
  public:
    typedef uint32_t Value;

    static uint32_t read(CMusic& music);
  };

public:
  typedef Binding<0x1E04, uint16_t> parametric_playSpeed;
  typedef Binding<0x1E05, uint16_t> parametric_byteRate;
  typedef Binding<0x1E06, uint16_t> parametric_endFillByte;
  typedef Binding<0x1E27, uint32_t> parametric_positionMsec;

  typedef Binding<0xC017, uint16_t> GPIO_DDR;
  typedef Binding<0xC040, uint16_t> I2S_CONFIG;
};


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic::Register (implementation)
//

inline void CMusic::Register::WriteUntilPin::wait(CMusic& music)
{
  while (music._pinRequest == LOW);
}


template<unsigned long TICKS>
inline void CMusic::Register::WriteUntilPinOrTimeout<TICKS>::wait(CMusic& music)
{
  static unsigned long const microsPerIncrement =  4;
  static unsigned long const ticksPerMicro      = 12;
  static unsigned long const ticksPerIncrement  = ticksPerMicro * microsPerIncrement;

  // convert timeout from ticks to microseconds,
  // round up to next minimum increment,
  // add one more minimum increment to handle aliasing

  static unsigned long const microsDeltaTimeout =
    (TICKS + ticksPerIncrement - 1) / ticksPerMicro + microsPerIncrement;

  unsigned long microsStart = micros();

  while (music._pinRequest == LOW && micros() - microsStart <= microsDeltaTimeout);
}


template<uint8_t ADDRESS, class TWriteUntil>
inline uint16_t CMusic::Register::Binding<ADDRESS, TWriteUntil>::read(CMusic& music)
{
  music._pinSelectControl = LOW;

  delayMicroseconds(1);

  SPI.transfer(SCI_OPCODE_READ);
  SPI.transfer(ADDRESS);

  uint16_t value =
      (uint16_t) SPI.transfer(0xFF) << 8
    | (uint16_t) SPI.transfer(0xFF) << 0;

  delayMicroseconds(1);

  music._pinSelectControl = HIGH;

  return value;
}


template<uint8_t ADDRESS, class TWriteUntil>
inline void CMusic::Register::Binding<ADDRESS, TWriteUntil>::write(CMusic& music, uint16_t value)
{
  music._pinSelectControl = LOW;

  delayMicroseconds(1);

  SPI.transfer(SCI_OPCODE_WRITE);
  SPI.transfer(ADDRESS);
  SPI.transfer((uint8_t) (value >> 8) & 0xFF);
  SPI.transfer((uint8_t) (value >> 0) & 0xFF);

  delayMicroseconds(1);

  TWriteUntil::wait(music);

  music._pinSelectControl = HIGH;
}


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic::Memory (implementation)
//

template<uint16_t ADDRESS>
uint16_t CMusic::Memory::Binding<ADDRESS, uint16_t>::read(CMusic& music)
{
  music.write<Register::SCI_WRAMADDR>(ADDRESS);
  return music.read<Register::SCI_WRAM>();
}


template<uint16_t ADDRESS>
void CMusic::Memory::Binding<ADDRESS, uint16_t>::write(CMusic& music, uint16_t value)
{
  music.write<Register::SCI_WRAMADDR>(ADDRESS);
  music.write<Register::SCI_WRAM>(value);
}


template<uint16_t ADDRESS>
uint32_t CMusic::Memory::Binding<ADDRESS, uint32_t>::read(CMusic& music)
{
  uint32_t valuePrev = 0xFFFFFFFF;

  for (;;) {
    music.write<Register::SCI_WRAMADDR>(ADDRESS);

    uint32_t valueNext =
        (uint32_t) music.read<Register::SCI_WRAM>() << 16
      | (uint32_t) music.read<Register::SCI_WRAM>() <<  0;

    if (valuePrev == valueNext)
      break;
    
    valuePrev = valueNext;
  }

  return valuePrev;
}


////////////////////////////////////////////////////////////////////////////////
//
//  TReadable (implementation)
//  TWritable (implementation)
//

template<class TReadable>
inline typename TReadable::Value CMusic::read()
{
  return TReadable::read(*this);
}


template<class TWritable>
inline void CMusic::write(typename TWritable::Value value)
{
  TWritable::write(*this, value);
}


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic::Buffer (implementation)
//

template<size_t SIZE>
inline CMusic::Buffer<SIZE>::Buffer()
  : _file   (0)
  , _offset (0)
  , _length (0)
{
  // nothing else to do
}


template<size_t SIZE>
inline void CMusic::Buffer<SIZE>::open(File& file)
{
  _file = &file;

  _offset = 0;
  _length = _file->read(_buffer, SIZE);
}


template<size_t SIZE>
void CMusic::Buffer<SIZE>::refill(size_t nBytesMin)
{
  if (!active() || _length >= nBytesMin)
    return;

  if (_length > 0 && _offset > 0) {
    unsigned char* byteDest   = _buffer;
    unsigned char* byteSource = _buffer + _offset;

    for (size_t iByte = 0; iByte < _length; ++iByte)
      *byteDest++ = *byteSource++;
  }

  _offset = 0;
  _length += _file->read(_buffer + _length, SIZE - _length);
}


template<size_t SIZE>
inline void CMusic::Buffer<SIZE>::close()
{
  _file   = 0;
  _offset = 0;
  _length = 0;
}


template<size_t SIZE>
inline size_t CMusic::Buffer<SIZE>::available() const
{
  return _length;
}


template<size_t SIZE>
inline unsigned char CMusic::Buffer<SIZE>::read()
{
  if (_length == 0)
    return 0;

  _length--;
  return _buffer[_offset++];
}


////////////////////////////////////////////////////////////////////////////////
//
//  CMusic (implementation)
//

CMusic::CMusic()
  : _cancel               (false)
  , _actionCancel         (ACTION_CANCEL_NONE)
  , _actionBuffer         (ACTION_BUFFER_NONE)
  , _nBytesFlushRemaining (0)
  , _volume               (255)
  , _balance              (0)
  , _msecPlaybackStart    (0)
{
  // nothing else to do
}


void CMusic::begin(uint8_t addressPinReset, uint8_t addressPinRequest, uint8_t addressPinSelectData, uint8_t addressPinSelectControl)
{
  _pinReset        .begin(addressPinReset,         HIGH);
  _pinRequest      .begin(addressPinRequest);
  _pinSelectData   .begin(addressPinSelectData,    HIGH);
  _pinSelectControl.begin(addressPinSelectControl, HIGH);

  reset();
}


void CMusic::reset(bool hardware, bool settings)
{
  if (hardware) {
    // hardware reset

    _pinSelectControl = HIGH;
    _pinSelectData    = HIGH;

    _pinReset = LOW;   delay(10);
    _pinReset = HIGH;  delay(10);

    while (_pinRequest == LOW);


    // set clock

    uint16_t saveSPCR = SPCR;
    uint16_t saveSPSR = SPSR;

    SPI.setClockDivider(SPI_CLOCK_DIV64);

    write<Register::SCI_CLOCKF>(
        0x4  << 13    // SC_MULT = 0b100  (set clock multiplier to 3.5)
      | 0x3  << 11    // SC_ADD  = 0b11   (set clock modification by decoder allowed to max)
      | 0x00 <<  0);  // SC_FREQ = 0      (indicate XTALI frequency is default 12.288 MHz)

    SPCR = saveSPCR;
    SPSR = saveSPSR;
  }


  // software reset

  write<Register::SCI_MODE>(
      Register::SM_SDINEW
    | Register::SM_RESET);

  while (_pinRequest == LOW);


  // enable I2S output

  write<Memory::GPIO_DDR>(
      0x1 << 7    // set pin GPIO7 (I2S_SDATA) to output
    | 0x1 << 6    // set pin GPIO6 (I2S_SCLK)  to output
    | 0x1 << 5    // set pin GPIO5 (I2S_MCLK)  to output
    | 0x1 << 4    // set pin GPIO4 (I2S_LROUT) to output
    | 0x0 << 3    // set pin GPIO3 to input
    | 0x0 << 2    // set pin GPIO2 to input
    | 0x0 << 1    // set pin GPIO1 to input
    | 0x0 << 0);  // set pin GPIO0 to input

  write<Memory::I2S_CONFIG>(
      0x1 << 3    // I2S_CF_MCLK_ENA = 0b1   (enable I2S_MCLK output)
    | 0x1 << 2    // I2S_CF_ENA      = 0b1   (enable I2S)
    | 0x0 << 0);  // I2S_CF_SRATE    = 0b00  (set I2S clock rate to 48 kHz)


  // reset playback state

  _buffer.close();

  _cancel = false;

  _actionCancel = ACTION_CANCEL_NONE;
  _actionBuffer = ACTION_BUFFER_NONE;

  _nBytesFlushRemaining = 0;

  if (settings) {
    _volume  = 255;
    _balance = 0;
  }
  else {
    updateVolumeAndBalance();
  }
}


bool CMusic::play(File& file)
{
  if (state() != STATE_IDLE)
    return false;

  _buffer.open(file);

  _actionCancel = ACTION_CANCEL_SET_AFTER_FLUSH;
  _actionBuffer = ACTION_BUFFER_NONE;

  _msecPlaybackStart = millis();

  return true;
}


bool CMusic::cancel()
{
  if (state() != STATE_PLAYING)
    return false;

  _actionCancel = ACTION_CANCEL_SET_IMMEDIATE;
  _actionBuffer = ACTION_BUFFER_CLOSE_AFTER_CANCEL;

  return true;
}


bool CMusic::loop(unsigned long msecMax)
{
  bool active = false;

  unsigned long msecStart = (msecMax != 0 ? millis() : 0);

  for (;;) {
    if (msecMax != 0 && millis() - msecStart > msecMax)
      return active;

    if (state() == STATE_IDLE
        && _actionCancel == ACTION_CANCEL_NONE
        && _actionBuffer == ACTION_BUFFER_NONE)
      return active;

    if (_cancel) {
      uint16_t mode = read<Register::SCI_MODE>();

      if (!(mode & Register::SM_CANCEL))
        _cancel = false;

      active = true;
    }
    else if (_actionCancel == ACTION_CANCEL_SET_IMMEDIATE) {
      uint16_t mode = read<Register::SCI_MODE>();

      if (!(mode & Register::SM_CANCEL))
        write<Register::SCI_MODE>(mode | Register::SM_CANCEL);

      _cancel = true;
      _actionCancel = ACTION_CANCEL_NONE;

      active = true;
    }

    if (_pinRequest == LOW)
      return active;

    active = true;

    if (_cancel) {
      size_t nBytesAudioSent = 0;

      if (_buffer.active()) {
        nBytesAudioSent = sendAudio(32);

        if (nBytesAudioSent < 32)
          _buffer.close();
      }

      if (nBytesAudioSent < 32)
        sendFlush(32 - nBytesAudioSent);
    }
    else {
      if (_actionBuffer == ACTION_BUFFER_CLOSE_AFTER_CANCEL) {
        _nBytesFlushRemaining = 2052;
        _buffer.close();
        _actionBuffer = ACTION_BUFFER_NONE;
      }

      size_t nBytesAudioSent = 0;

      if (_buffer.active()) {
        nBytesAudioSent = sendAudio(32);

        if (nBytesAudioSent < 32) {
          _nBytesFlushRemaining = 2052;
          _buffer.close();
        }
      }

      if (_nBytesFlushRemaining > 0) {
        size_t nBytesFlushSent = sendFlush(32 - nBytesAudioSent);

        if (_nBytesFlushRemaining > nBytesFlushSent) {
          _nBytesFlushRemaining -= nBytesFlushSent;
        }
        else {
          _nBytesFlushRemaining = 0;
          if (_actionCancel == ACTION_CANCEL_SET_AFTER_FLUSH)
            _actionCancel = ACTION_CANCEL_SET_IMMEDIATE;
        }
      }
    }

    _buffer.refill(32);
  }
}


inline size_t CMusic::sendAudio(size_t nBytesMax)
{
  size_t nBytesRead = _buffer.available();

  if (nBytesRead > nBytesMax)
    nBytesRead = nBytesMax;

  _pinSelectData = LOW;

  for (size_t iByte = 0; iByte < nBytesRead; ++iByte)
    SPI.transfer(_buffer.read());

  _pinSelectData = HIGH;

  return nBytesRead;
}


inline size_t CMusic::sendFlush(size_t nBytesMax)
{
  _pinSelectData = LOW;

  for (size_t iByte = 0; iByte < nBytesMax; ++iByte)
    SPI.transfer(0x00);
  
  _pinSelectData = HIGH;

  return nBytesMax;
}


void CMusic::updateVolumeAndBalance()
{
  // SCI_VOL expects relative sound pressure level in units of -0.5 dB,
  // going from 0 dB (max loudness x 1) to -127.5 dB (x 0.00015).
  //
  // Since dB are an unintuitive way to specify subjective loudness,
  // use a lookup table to convert a linear amplitude scale that goes
  // from 0 (silence) to 255 (max loudness) to the expected dB value.
  //
  // loudness / 255 = 2 ** (10 * (SCI_VOL / -0.5))
  // SCI_VOL = -(1/0.5 * 10 * log[2](loudness / 255))

  static uint8_t const level[32] PROGMEM = {
    /* loudness =  0 ..  7 */  254,  99,  79,  67,  59,  53,  47,  43,
    /* loudness =  8 .. 15 */   39,  36,  33,  30,  27,  25,  23,  21,
    /* loudness = 16 .. 23 */   19,  17,  16,  14,  13,  11,  10,   9,
    /* loudness = 24 .. 31 */    7,   6,   5,   4,   3,   2,   1,   0,
  };

  uint8_t attenuationLeft  = (_balance > 0 ? (int16_t) _volume * _balance /  127 : 0);
  uint8_t attenuationRight = (_balance < 0 ? (int16_t) _volume * _balance / -128 : 0);

  uint8_t volumeLeft  = (_volume > attenuationLeft  ? _volume - attenuationLeft  : 0);
  uint8_t volumeRight = (_volume > attenuationRight ? _volume - attenuationRight : 0);

  uint16_t levelCombined =
      (uint16_t) pgm_read_byte(level + (volumeLeft  / 8)) << 8
    | (uint16_t) pgm_read_byte(level + (volumeRight / 8)) << 0;

  write<Register::SCI_VOL>(levelCombined);
}
