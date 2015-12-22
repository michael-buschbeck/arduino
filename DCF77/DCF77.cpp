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


#include "DCF77.h"


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Receiver (implementation)
//

void DCF77Receiver::processSample(bool const sample, uint32_t const time)
{
  this->windowPulse <<= 1;
  this->windowPulse |= (sample ? 1 : 0);

  // Keep an average per sample point in a second over nSecondToSmooth seconds
  // in order to emphasize the leading pulse edge (which is always at the same
  // point each second, except in the last second of a minute) and to smoothen
  // out any one-off sampling errors.
  //
  // Update sumsSamples (which has one 4-bit slot per sample point in a second):
  //
  //   - subtract the sample value that will be replaced,
  //   - add the sample value it will be replaced with.
  //
  // This makes updating the average per sample point a trivial operation that
  // involves changing (at most) one 4-bit sum and a single sample bit at the
  // expense of having to provide enough memory to store all samples that are
  // part of the average.

  bool const samplePrev = this->bitptrSamples.get(this->samples);

  uint8_t sumSamples;

  if (samplePrev == sample) {
    sumSamples = (this->sumsSamples[this->indexSumSamples] & this->bitmaskSumSamples);
  }
  else {
    // Either samplePrev or sample is set at this point
    if (samplePrev)
           this->sumsSamples[this->indexSumSamples] -= (0b00010001 & this->bitmaskSumSamples);
      else this->sumsSamples[this->indexSumSamples] += (0b00010001 & this->bitmaskSumSamples);

    sumSamples = (this->sumsSamples[this->indexSumSamples] & this->bitmaskSumSamples);

    // Store sample in order to subtract it from sumsSamples later
    this->bitptrSamples.set(this->samples, sample);
  }

  ++this->bitptrSamples;

  if (this->bitptrSamples == nSamplePerSecond * nSecondToSmooth)
    this->bitptrSamples = 0;

  // 0x00001111 => 0b11110000 => 0b00001111 => ...
  this->bitmaskSumSamples = ~this->bitmaskSumSamples;

  if (this->bitmaskSumSamples == 0b00001111) {
    // 0 => 1 => ... => sizeof(sumsSamples)-1 => 0 => 1 => ...
    this->indexSumSamples += 1;
    if (this->indexSumSamples == sizeof(this->sumsSamples))
      this->indexSumSamples = 0;
  }

  // sumSamples can have the real sum in either the upper or the lower nibble;
  // for example, assuming the sum is 6 = 0b0110, either of the following cases
  // is possible (depending on the current bitmaskSumSamples):
  //
  //   - sumSamples = 0b00000110 when bitmaskSumSamples = 0b00001111
  //   - sumSamples = 0b01100000 when bitmaskSumSamples = 0b11110000
  //     
  // The general condition for sampleSmooth to be true is this:
  //
  //     sampleSmooth = (sum > nSecondToSmooth / 2)
  //
  // Given the constant value nSecondToSmooth = 15, this boils down to the
  // following condition:
  //
  //     sampleSmooth = (sum > 7)
  //                  = (sum > 0b0111)
  //                  = (sum & 0b1000 != 0)
  //
  // So the comparison against the unshifted sumSamples (where the actual sum
  // might be in either of its nibbles) can be reduced to a simple bit test.

  bool sampleSmooth = ((sumSamples & 0b10001000) != 0);

  this->windowSmooth <<= 1;
  this->windowSmooth |= (sampleSmooth ? 1 : 0);

  // Edge detection
  if ((this->windowSmooth & 0b1111110000) == 0b1110000000) {
    uint32_t timeEdgeDelta = time - this->timeEdge;

    if (timeEdgeDelta < 500) {
      // Too soon, so ignore this edge
    }
    else {
      if (timeEdgeDelta > 1500) {
        // Too late, therefore missed at least one edge
        this->bitptrTimebits = 0;
      }

      this->timeEdge = time;

      // Count the un-smoothed sample bits that constitute the pulse proper,
      // i.e. those beyond the leading bits used for edge detection.
      // 
      //     windowPulse = 0b1111111111 -> last second before changeover
      //     windowPulse = 0b1110001111 -> short pulse -> time bit = 0
      //     windowPulse = 0b1110000001 -> long  pulse -> time bit = 1
      //                     |  |  |  |
      //     pre-edge--------+  |  |  |
      //     pulse 1st half = 0-+  |  |
      //     pulse 2nd half = X----+  |
      //     post-pulse bonus sample--+
      //
      // Since this algorithm wants to be robust against sampling errors, bits
      // are counted and compared against thresholds rather than compared to
      // idealized patterns (such as the ones shown above).
      //
      // First, a distinction is made between having a pulse or having none.
      // Since edge detection smoothes over many seconds, an edge is detected
      // even if there is no actual pulse in the unsmoothed sample data.
      // 
      // If there are more than nSampleThresholdMinute = 5 one-bits (or, looked
      // at it from another perspective, 2 or fewer zero-bits) in the six pulse
      // samples plus the post-pulse bonus sample, a non-pulse is detected,
      // which signifies that the next pulse signifies a minute changeover.

      // Need only the lower 7 bits of windowPulse
      uint8_t samplesPulse = (*(reinterpret_cast<uint8_t*>(&this->windowPulse)) & 0b01111111);

      uint8_t sumSamplesPulse;
      for (sumSamplesPulse = 0; samplesPulse != 0; ++sumSamplesPulse)
        samplesPulse &= (samplesPulse - 1);

      if (sumSamplesPulse > nSampleThresholdMinute) {
        this->changeover = true;
      }
      else {
        if (this->changeover) {
          if (this->bitptrTimebits == 59 && this->callbackReceived) {
            DCF77Timestamp timestamp (this->timebits);
            this->callbackReceived(timestamp);
          }

          this->changeover = false;
          this->bitptrTimebits = 0;
        }

        // Stop decoding time bits if more than one minute has passed since the
        // last changeover had been detected. The time bits gathered so far are
        // only going to be decoded if we got exactly 59 of them (leap seconds
        // nonwithstanding since they are so rare).

        if (this->bitptrTimebits != 60) {
          // Count the one-bits in the second half of the pulse plus the bonus
          // sample. If there are more one-bits than nSampleThresholdZero = 1,
          // this is a short pulse (time bit = 0), else a long pulse (1).

          // Need only the lower 4 bits of windowPulse
          uint8_t samplesTrail = (*(reinterpret_cast<uint8_t*>(&this->windowPulse)) & 0b00001111);

          uint8_t sumSamplesTrail;
          for (sumSamplesTrail = 0; samplesTrail != 0; ++sumSamplesTrail)
            samplesTrail &= (samplesTrail - 1);
          
          bool timebit = (sumSamplesTrail <= nSampleThresholdZero);
          this->bitptrTimebits.set(this->timebits, timebit);

          ++this->bitptrTimebits;
        }
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
//
//  DCF77Timestamp (implementation)
//


//  The DCF77 time data structure:
//
//
//  byte  0....... 1....... 2....... 3....... 4....... 5....... 6....... 7.......
//        |        |        |        |        |        |        |        |       
//  data  0------- -------- -zz-1nnn nNNNphhh hHHpdddd DDwwwmmm mMMyyyyY YYYp
//        |                  |  ||        |       |      |  |      |        |
//        constant           |  ||        |       |      |  |      |        |
//        CET/CEST-----------+  ||        |       |      |  |      |        |
//        constant--------------+|        |       |      |  |      |        |
//        BCD minute and parity--+        |       |      |  |      |        |
//        BCD hour and parity-------------+       |      |  |      |        |
//        BCD day---------------------------------+      |  |      |        |
//        day of week------------------------------------+  |      |        |
//        BCD month-----------------------------------------+      |        |
//        BCD year-------------------------------------------------+        |
//        date parity-------------------------------------------------------+
//
//
//  Details on how to interpret each field (and its range restrictions, which
//  play an important part in discarding invalid data):
//
//      https://en.wikipedia.org/wiki/DCF77#Time_code_interpretation


union DCF77Timestruct
{
  struct
  {
    uint8_t  startMinute      :  1;  // bit 0
    uint16_t weather          : 14;  // bits 1-14
    bool     call             :  1;  // bit 15
    bool     changeoverZone   :  1;  // bit 16
    uint8_t  zone             :  2;  // bits 17-18
    bool     leap             :  1;  // bit 19
    uint8_t  startTime        :  1;  // bit 20
    uint8_t  minute01         :  4;  // bit 21-24
    uint8_t  minute10         :  3;  // bit 25-27
    uint8_t  parityMinute     :  1;  // bit 28
    uint8_t  hour01           :  4;  // bits 29-32
    uint8_t  hour10           :  2;  // bits 33-34
    uint8_t  parityHour       :  1;  // bit 35
    uint8_t  day01            :  4;  // bits 36-39
    uint8_t  day10            :  2;  // bits 40-41
    uint8_t  weekday          :  3;  // bits 42-44
    uint8_t  month01          :  4;  // bits 45-48
    uint8_t  month10          :  1;  // bit 49
    uint8_t  year01           :  4;  // bits 50-53
    uint8_t  year10           :  4;  // bits 54-57
    uint8_t  parityDate       :  1;  // bit 58
  };

  struct
  {
    uint32_t                  : 21;  // bits 0-20
    uint8_t  bitsParityMinute :  8;  // bits 21-28
    uint8_t  bitsParityHour   :  7;  // bits 29-35
    uint8_t  bitsParityDate1  :  4;  // bits 36-39
    uint8_t  bitsParityDate2  :  8;  // bits 40-47
    uint8_t  bitsParityDate3  :  8;  // bits 48-55
    uint8_t  bitsParityDate4  :  3;  // bits 56-58
  };
};


DCF77Timestamp::Status DCF77Timestamp::decode(uint8_t const timebits[8])
{
  const DCF77Timestruct& timestruct = reinterpret_cast<const DCF77Timestruct&>(*timebits);

  this->status = INCOMPLETE;

  // Start of minute:
  // Always zero.

  if (timestruct.startMinute != 0)
    return (this->status = ERROR_START_MINUTE);

  // DST:
  // Encoded in two bits, one each for CEST (DST) and CET, even though in
  // principle only one bit of information is encoded. Validate by checking for
  // invalid combinations.

  switch (timestruct.zone) {
    case 0b01:  this->time.dst = true;   break;
    case 0b10:  this->time.dst = false;  break;
    default:    return (this->status = ERROR_DST);
  }

  // Start of time and date:
  // Always one.

  if (timestruct.startTime != 1)
    return (this->status = ERROR_START_TIME);

  // Minute:
  // Validate range of individual BCD digits, and verify even parity across
  // all minute bits including the parity bit.

  if (timestruct.minute01 > 9)
    return (this->status = ERROR_MINUTE_01_RANGE);
  if (timestruct.minute10 > 5)
    return (this->status = ERROR_MINUTE_10_RANGE);

  this->time.minute = timestruct.minute10 * 10 + timestruct.minute01;

  uint8_t bitsParityMinute = timestruct.bitsParityMinute;

  bool incorrectParityMinute;
  for (incorrectParityMinute = false; bitsParityMinute != 0; incorrectParityMinute = !incorrectParityMinute)
    bitsParityMinute &= (bitsParityMinute - 1);

  if (incorrectParityMinute)
    return (this->status = ERROR_MINUTE_PARITY);

  // Hour:
  // Validate range of individual BCD digits, range of the result (which can
  // stil be out of range even though each digit is in range), and verify even
  // parity across all hour bits including the parity bit.

  if (timestruct.hour01 > 9)
    return (this->status = ERROR_HOUR_01_RANGE);
  if (timestruct.hour10 > 2)
    return (this->status = ERROR_HOUR_10_RANGE);

  this->time.hour = timestruct.hour10 * 10 + timestruct.hour01;

  if (this->time.hour > 23)
    return (this->status = ERROR_HOUR_RANGE);

  uint8_t bitsParityHour = timestruct.bitsParityHour;

  bool incorrectParityHour;
  for (incorrectParityHour = false; bitsParityHour != 0; incorrectParityHour = !incorrectParityHour)
    bitsParityHour &= (bitsParityHour - 1);

  if (incorrectParityHour)
    return (this->status = ERROR_HOUR_PARITY);
  
  // Day:
  // Validate range of BCD one-digit (but not of the BCD ten-digit, which is
  // naturally restricted to 0..3 because it is represented by two bits) and
  // range of the result (though not going as far as cross-verifying against
  // the per-month maximum number of days).

  if (timestruct.day01 > 9)
    return (this->status = ERROR_DAY_01_RANGE);
  
  this->date.day = timestruct.day10 * 10 + timestruct.day01;

  if (this->date.day == 0 || this->date.day > 31)
    return (this->status = ERROR_DAY_RANGE);

  // Weekday:
  // Validate range.

  this->date.weekday = timestruct.weekday;

  if (this->date.weekday == 0)
    return (this->status = ERROR_WEEKDAY_RANGE);

  // Month:
  // Validate range of BCD one-digit (but not of the BCD ten-digit, which is
  // naturally restricted to 0..1 because it is represented by one bit) and
  // range of the result.

  if (timestruct.month01 > 9)
    return (this->status = ERROR_MONTH_01_RANGE);

  this->date.month = timestruct.month10 * 10 + timestruct.month01;

  if (this->date.month == 0 || this->date.month > 12)
    return (this->status = ERROR_MONTH_RANGE);

  // Year:
  // Validate range of individual BCD digits.

  if (timestruct.year01 > 9)
    return (this->status = ERROR_YEAR_01_RANGE);
  if (timestruct.year10 > 9)
    return (this->status = ERROR_YEAR_10_RANGE);

  this->date.year = timestruct.year10 * 10 + timestruct.year01;

  // Parity of all date fields:
  // Condense the bits to calculate the parity of into a single uint8_t first
  // by xoring them onto each other. Doing that retains the parity, but reduces
  // the actual amount of bit counting that needs to be done to validate.

  uint8_t bitsParityDate =
      timestruct.bitsParityDate1
    ^ timestruct.bitsParityDate2
    ^ timestruct.bitsParityDate3
    ^ timestruct.bitsParityDate4;

  bool incorrectParityDate;
  for (incorrectParityDate = false; bitsParityDate != 0; incorrectParityDate = !incorrectParityDate)
    bitsParityDate &= (bitsParityDate - 1);

  if (incorrectParityDate)
    return (this->status = ERROR_DATE_PARITY);

  return (this->status = VALID);
}


void DCF77Timestamp::print(bool const includeStatus)
{
  switch (this->date.weekday) {
    case 1:  Serial.print(F("Mon "));  break;
    case 2:  Serial.print(F("Tue "));  break;
    case 3:  Serial.print(F("Wed "));  break;
    case 4:  Serial.print(F("Thu "));  break;
    case 5:  Serial.print(F("Fri "));  break;
    case 6:  Serial.print(F("Sat "));  break;
    case 7:  Serial.print(F("Sun "));  break;
  }

  if (this->date.day    < 10) Serial.print('0');  Serial.print(this->date.day);    Serial.print('.');
  if (this->date.month  < 10) Serial.print('0');  Serial.print(this->date.month);  Serial.print('.');
  if (this->date.year   < 10) Serial.print('0');  Serial.print(this->date.year);   Serial.print(' ');

  if (this->time.hour   < 10) Serial.print('0');  Serial.print(this->time.hour);   Serial.print(':');
  if (this->time.minute < 10) Serial.print('0');  Serial.print(this->time.minute); Serial.print(' ');

  Serial.print(this->time.dst ? F("CEST") : F("CET"));

  if (includeStatus) {
    Serial.print(F(" ("));

    switch (this->status) {
      case DCF77Timestamp::INCOMPLETE:             Serial.print(F("INCOMPLETE"));             break;
      case DCF77Timestamp::VALID:                  Serial.print(F("VALID"));                  break;
      case DCF77Timestamp::VALID_CONSUMED:         Serial.print(F("VALID_CONSUMED"));         break;
      case DCF77Timestamp::ERROR_START_MINUTE:     Serial.print(F("ERROR_START_MINUTE"));     break;
      case DCF77Timestamp::ERROR_DST:              Serial.print(F("ERROR_DST"));              break;
      case DCF77Timestamp::ERROR_START_TIME:       Serial.print(F("ERROR_START_TIME"));       break;
      case DCF77Timestamp::ERROR_MINUTE_01_RANGE:  Serial.print(F("ERROR_MINUTE_01_RANGE"));  break;
      case DCF77Timestamp::ERROR_MINUTE_10_RANGE:  Serial.print(F("ERROR_MINUTE_10_RANGE"));  break;
      case DCF77Timestamp::ERROR_MINUTE_PARITY:    Serial.print(F("ERROR_MINUTE_PARITY"));    break;
      case DCF77Timestamp::ERROR_HOUR_01_RANGE:    Serial.print(F("ERROR_HOUR_01_RANGE"));    break;
      case DCF77Timestamp::ERROR_HOUR_10_RANGE:    Serial.print(F("ERROR_HOUR_10_RANGE"));    break;
      case DCF77Timestamp::ERROR_HOUR_RANGE:       Serial.print(F("ERROR_HOUR_RANGE"));       break;
      case DCF77Timestamp::ERROR_HOUR_PARITY:      Serial.print(F("ERROR_HOUR_PARITY"));      break;
      case DCF77Timestamp::ERROR_DAY_01_RANGE:     Serial.print(F("ERROR_DAY_01_RANGE"));     break;
      case DCF77Timestamp::ERROR_DAY_RANGE:        Serial.print(F("ERROR_DAY_RANGE"));        break;
      case DCF77Timestamp::ERROR_WEEKDAY_RANGE:    Serial.print(F("ERROR_WEEKDAY_RANGE"));    break;
      case DCF77Timestamp::ERROR_MONTH_01_RANGE:   Serial.print(F("ERROR_MONTH_01_RANGE"));   break;
      case DCF77Timestamp::ERROR_MONTH_RANGE:      Serial.print(F("ERROR_MONTH_RANGE"));      break;
      case DCF77Timestamp::ERROR_YEAR_01_RANGE:    Serial.print(F("ERROR_YEAR_01_RANGE"));    break;
      case DCF77Timestamp::ERROR_YEAR_10_RANGE:    Serial.print(F("ERROR_YEAR_10_RANGE"));    break;
      case DCF77Timestamp::ERROR_DATE_PARITY:      Serial.print(F("ERROR_DATE_PARITY"));      break;
    }

    Serial.print(F(")"));
  }
}
