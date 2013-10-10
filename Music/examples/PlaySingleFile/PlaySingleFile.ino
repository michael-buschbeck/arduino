#include <SPI.h>
#include <SD.h>

#include "Music.h"
#include "Pin.h"


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

  // start playback!
  Music.play(fileMusic);
}


void loop()
{
  // make sure this is called frequently!
  Music.loop();

  // plenty of opportunity to do other stuff here...
}
