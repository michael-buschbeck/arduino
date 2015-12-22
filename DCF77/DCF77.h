///////////////////////////////////////////////////////////////////////////////
//
//  Robust DCF77 time data receiver library.
//
//  (C) 2015 Michael Buschbeck <michael@buschbeck.net>
//
//  Licensed under a Creative Commons Attribution 4.0 International License.
//  Distributed in the hope that it will be useful, but without any warranty.
//
//  See <http://creativecommons.org/licenses/by/4.0/> for details.
//


#ifndef DCF77_H
#define DCF77_H

#include <Arduino.h>


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Timestamp
//

class DCF77Timestamp
{
public:
  enum Status
  {
    INCOMPLETE,

    VALID,
    VALID_CONSUMED,  // for client use

    ERROR_START_MINUTE,
    ERROR_DST,
    ERROR_START_TIME,
    ERROR_MINUTE_01_RANGE,
    ERROR_MINUTE_10_RANGE,
    ERROR_MINUTE_PARITY,
    ERROR_HOUR_01_RANGE,
    ERROR_HOUR_10_RANGE,
    ERROR_HOUR_RANGE,
    ERROR_HOUR_PARITY,
    ERROR_DAY_01_RANGE,
    ERROR_DAY_RANGE,
    ERROR_WEEKDAY_RANGE,
    ERROR_MONTH_01_RANGE,
    ERROR_MONTH_RANGE,
    ERROR_YEAR_01_RANGE,
    ERROR_YEAR_10_RANGE,
    ERROR_DATE_PARITY
  };

  Status status;
  
  struct {
    uint8_t minute;   // 0 .. 59
    uint8_t hour;     // 0 .. 23
    bool    dst;      // false (CET), true (CEST)
  } time;

  struct {
    uint8_t day;      // 1 .. 31
    uint8_t weekday;  // 1 (Mon) .. 7 (Sun)
    uint8_t month;    // 1 .. 12
    uint8_t year;     // 0 .. 99
  } date;

  DCF77Timestamp();
  DCF77Timestamp(uint8_t const timebits[8]);

  Status decode(uint8_t const timebits[8]);

  void print(bool const includeStatus = false);
};


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Receiver
//

class DCF77Receiver
{
public:
  typedef void (*TCallbackReceived)(DCF77Timestamp const & timestamp);

  DCF77Receiver(TCallbackReceived const callbackReceived = 0);

  void processSample(bool const sample, uint32_t time);


  class Bitptr
  {
  public:
    Bitptr(uint16_t const position = 0);

    Bitptr& operator++();
    Bitptr& operator=(uint16_t const position);

    bool operator==(Bitptr const bitptr) const;
    bool operator!=(Bitptr const bitptr) const;
    bool operator==(uint16_t const position) const;
    bool operator!=(uint16_t const position) const;

    bool get(uint8_t const * const bytes) const;
    void set(uint8_t * const bytes, bool const bit) const;

  private:
    uint8_t index;
    uint8_t bitmask;
  };


  // Do not change these constants: They have been tuned based on real-life
  // sampling data; and the processing code is, in some placed, optimized on
  // the assumption that these values are exactly as set here (see below).

  static constexpr uint8_t nSamplePerSecond       = 32;
  static constexpr uint8_t nSecondToSmooth        = 15;
               
  static constexpr uint8_t nSampleInWindow        = 10;
               
  static constexpr uint8_t nSampleThresholdZero   =  1;
  static constexpr uint8_t nSampleThresholdMinute =  5;


  // All raw samples that are part of the moving averages.

  uint8_t samples[nSecondToSmooth * nSamplePerSecond / (8/1)];
  Bitptr bitptrSamples;


  // Sums of all samples for each sampling point in a second. Used to average
  // across a moving interval of nSecondToSmooth = 15 seconds. Each sum has the
  // range 0..15 and occupies 4 bits (one nibble) in the sumsSamples array.

  uint8_t sumsSamples[nSamplePerSecond / (8/4)];
  uint8_t indexSumSamples;
  uint8_t bitmaskSumSamples;


  // Store a moving window of the last nSampleInWindow = 10 samples (actually
  // the last 16 samples since these are uint16_t values, but only 10 samples
  // are ever used).
  //
  // The data stored here is obviously redundant with the data already stored
  // in samples and sumsSamples, but having the most recent few values that are
  // relevant for edge (windowSmooth) and pulse type (windowPulse) detection in
  // these variables as well makes processing a lot easier and faster.

  uint16_t windowSmooth;
  uint16_t windowPulse;


  // Internal time, in milliseconds, when most recently an edge was detected.
  // Used to discriminate against detection of bogus edges that are too close
  // to each other, and to find out when an edge had been missed.

  uint32_t timeEdge;


  // Actual raw time data bits decoded from the detected pulses.
  //
  // One full minute's worth of raw time data bits comprises exactly 59 bits;
  // except in the very rare case of having a leap second, when a zero bit is
  // added as the 60th bit.
  //
  // Ignoring leap seconds, recording of time data bits stops after 60 bits
  // have been gathered since this means that the end-of-minute non-pulse has
  // been skipped accidentally.

  uint8_t timebits[8];
  Bitptr bitptrTimebits;


  // Indicates that the next detected pulse signifies the start of the next
  // minute, at which point the time described by the (hopefully exactly 59,
  // and hopefully internally consistent) raw time data bits that have been
  // gathered come into effect as the actual current time.

  bool changeover;


  // Callback to invoke whenever a complete timestamp is received.
  //
  // Note that the timestamp does not necessarily have to be valid; check its
  // status field to find out if it is (or why it is not).
  //
  // Although it is arguably pointless to do anything with invalid timestamps,
  // including invoking a callback to receive them, the rationale behind doing
  // it anyway is that maybe some callbacks will be happy with getting a valid
  // time even though the date may be broken; and it also helps with debugging.
  //
  // The callback will be invoked in the very moment the start-of-minute pulse
  // has been detected. Most importantly, this means that it will be invoked in
  // the middle of processing a sample; if this is done via a timer interrupt,
  // keep in mind that there is plenty of stuff you must not do while in the
  // middle of an interrupt.

  TCallbackReceived callbackReceived;
};


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Receiver::Bitptr (implementation)
//

inline DCF77Receiver::Bitptr& DCF77Receiver::Bitptr::operator++()
{
  bitmask <<= 1;

  if (this->bitmask == 0) {
    this->index += 1;
    this->bitmask = 1;
  }

  return *this;
}


inline DCF77Receiver::Bitptr& DCF77Receiver::Bitptr::operator=(uint16_t const position)
{
  this->index   = (position >> 3);
  this->bitmask = (1 << (position & 7));

  return *this;
}


inline bool DCF77Receiver::Bitptr::operator==(DCF77Receiver::Bitptr const bitptr) const
{
  return (this->index == bitptr.index && this->bitmask == bitptr.bitmask);
}


inline bool DCF77Receiver::Bitptr::operator!=(DCF77Receiver::Bitptr const bitptr) const
{
  return (this->index != bitptr.index || this->bitmask != bitptr.bitmask);
}


inline bool DCF77Receiver::Bitptr::operator==(uint16_t const position) const
{
  return (this->index == (position >> 3) && this->bitmask == (1 << (position & 7)));
}


inline bool DCF77Receiver::Bitptr::operator!=(uint16_t const position) const
{
  return (this->index != (position >> 3) || this->bitmask != (1 << (position & 7)));
}


inline bool DCF77Receiver::Bitptr::get(uint8_t const * const bytes) const
{
  return ((bytes[this->index] & this->bitmask) != 0);
}


inline void DCF77Receiver::Bitptr::set(uint8_t * const bytes, bool const bit) const
{
  if (bit)
         bytes[this->index] |=  this->bitmask;
    else bytes[this->index] &= ~this->bitmask;
}


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Timestamp (implementation)
//

inline DCF77Timestamp::DCF77Timestamp()
  : status (INCOMPLETE)
{}


inline DCF77Timestamp::DCF77Timestamp(uint8_t const timebits[8])
{
  this->decode(timebits);
}


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Receiver (implementation)
//

inline DCF77Receiver::DCF77Receiver(DCF77Receiver::TCallbackReceived const callbackReceived)
  : bitptrSamples     (0)
  , indexSumSamples   (0)
  , bitmaskSumSamples (0b00001111)
  , windowSmooth      (0b0000000000)
  , windowPulse       (0b0000000000)
  , timeEdge          (0)
  , bitptrTimebits    (0)
  , changeover        (false)
  , callbackReceived  (callbackReceived)
{}


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Receiver::Bitptr (implementation)
//

inline DCF77Receiver::Bitptr::Bitptr(uint16_t const position)
  : index   (position >> 3)
  , bitmask (1 << (position & 7))
{}


#endif
