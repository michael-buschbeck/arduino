#include <SPI.h>
#include <SD.h>

#include "Music.h"
#include "Pin.h"
#include "Timer.h"


// multifunction button pressed down:
// react on falling edge only (start or stop playback)
PinTrigger<FALLING> pinTriggerPlay         (5, HIGH);

// multifunction button tilted up/down and left/right
// react on both rising and falling edge (start/stop timer)
PinTrigger<CHANGE>  pinTriggerVolumeUp     (3, HIGH);
PinTrigger<CHANGE>  pinTriggerVolumeDown   (7, HIGH);
PinTrigger<CHANGE>  pinTriggerBalanceLeft  (6, HIGH);
PinTrigger<CHANGE>  pinTriggerBalanceRight (4, HIGH);

// auto-repeating timers to have volume and balance shift continuously
// for as long as the corresponding button is activated
Timer timerVolumeUp     (Timer::IMMEDIATE | Timer::REPEAT, 100);
Timer timerVolumeDown   (Timer::IMMEDIATE | Timer::REPEAT, 100);
Timer timerBalanceLeft  (Timer::IMMEDIATE | Timer::REPEAT, 100);
Timer timerBalanceRight (Timer::IMMEDIATE | Timer::REPEAT, 100);

// music file from SD card to play back
File fileMusic;


void setup()
{
  // initialize serial output
  Serial.begin(9600);

  // initialize SPI communications
  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV4);

  // initialize SD card reader
  SD.begin();

  // music file in SD card root directory;
  // keep in mind that this must be an 8.3 file name!
  fileMusic = SD.open("JAZZ.MP3");

  // initialize Music library
  Music.begin();
  Music.volume(192);
}


void changeAndShowMusicVolume(uint8_t volume)
{
  Music.volume(volume);

  Serial.print('|');
  for (uint8_t volumeDisplayed = 0; volumeDisplayed < 248; volumeDisplayed += 8)
    Serial.print(volumeDisplayed < volume ? '#' : '-');
  Serial.println('|');
}


void changeAndShowMusicBalance(int8_t balance)
{
  Music.balance(balance);

  Serial.print('|');
  for (int8_t balanceDisplayed = -128; balanceDisplayed < 120; balanceDisplayed += 8)
    Serial.print(balanceDisplayed < balance && balance <= balanceDisplayed + 8 ? '#' : '-');
  Serial.println('|');
}


void loop()
{
  // make sure this is called frequently!
  Music.loop();

  // multifunction button pressed down:
  // start or stop playing the music file
  if (pinTriggerPlay) {

    // react depending on the current playback state
    switch (Music.state()) {

      // not playing anything yet? then start playback
      case MUSIC_STATE_IDLE:
        Serial.println(F("starting playback..."));
        // reset playback position to start of file
        fileMusic.seek(0);
        // start playback of the music file
        Music.play(fileMusic);
        break;

      // playback in progress? then stop it
      case MUSIC_STATE_PLAYING:
        Serial.println(F("cancelling playback..."));
        // cancel playback of the music file
        Music.cancel();
        break;

      // busy? ignore (should be rather unlikely)
      case MUSIC_STATE_BUSY:
        break;
    }
  } 

  // multifunction button tilted up:
  // start/stop auto-repeating timer to raise volume
  switch (pinTriggerVolumeUp) {
    case FALLING: timerVolumeUp.start(); break;
    case RISING:  timerVolumeUp.stop();  break;
  }

  if (timerVolumeUp.due()) {
    uint8_t volume = Music.volume();
    if (volume < 248)
      changeAndShowMusicVolume(volume + 8);
  }

  // multifunction button tilted down:
  // start/stop auto-repeating timer to lower volume
  switch (pinTriggerVolumeDown) {
    case FALLING: timerVolumeDown.start(); break;
    case RISING:  timerVolumeDown.stop();  break;
  }

  if (timerVolumeDown.due()) {
    uint8_t volume = Music.volume();
    if (volume >= 8)
      changeAndShowMusicVolume(volume - 8);
  }

  // multifunction button tilted to the left
  // start/stop auto-repeating timer to shift balance left
  switch (pinTriggerBalanceLeft) {
    case FALLING: timerBalanceLeft.start(); break;
    case RISING:  timerBalanceLeft.stop();  break;
  }

  if (timerBalanceLeft.due()) {
    int8_t balance = Music.balance();
    if (balance > -120)
      changeAndShowMusicBalance(balance - 8);
  }

  // multifunction button tilted to the right:
  // start/stop auto-repeating timer to shift balance right
  switch (pinTriggerBalanceRight) {
    case FALLING: timerBalanceRight.start(); break;
    case RISING:  timerBalanceRight.stop();  break;
  }

  if (timerBalanceRight.due()) {
    int8_t balance = Music.balance();
    if (balance < 120)
      changeAndShowMusicBalance(balance + 8);
  }
}
