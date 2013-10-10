This is a non-blocking library for [VS1053b](http://www.vlsi.fi/en/products/vs1053.html)-based Arduino expansion boards such as the [Seeedstudio Music Shield v2.0](http://www.seeedstudio.com/wiki/Music_Shield_V2.0).



Why?
----

The library that comes with Seeedstudio's Music Shield blocks until it's done playing  so you can *either* play music *or* do anything else. Since I wanted to use the Music Shield to play some sound effects in parallel to doing other exciting stuff (like blinking an LED!), that just didn't do.



How?
----

### Installation

Download the `Music`, `Pin` and `Timer` directories and place them in the `libraries` folder in your Arduino Sketchbook directory. (The `Timer` library is only used by the `PlayWithControls` example. You don't need it if you don't want to compile and upload that particular example.)


### Usage

Include the [SD](http://arduino.cc/en/Reference/SD) and [SPI](http://arduino.cc/en/Reference/SPI) libraries, the Music library and the Pin library in your sketch:

    #include <SD.h>
    #include <SPI.h>
    #include "Music.h"
    #include "Pin.h"

**Do not** include Seeedstudio's `MusicPlayer.h` library as well  they'll both try do use the same resources, and that won't work. (In particular, the Music library uses Arduino's standard [SD](http://arduino.cc/en/Reference/SD) and [SPI](http://arduino.cc/en/Reference/SPI) libraries, while Seeedstudio's library uses its own libraries for that.)

In your `setup()` routine, initialize everything:

    // initialize SPI communications
    SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV4);

    // initialize SD card reader
    SD.begin();

    // initialize the Music library,
    // using the default pins:
    // A0 - reset
    // A1 - data request
    // A2 - SPI slave select: data
    // A3 - SPI slave select: control
    Music.begin();

There's also a variant of the `Music.begin()` method that explicitly takes those four pin addresses, just in case you'd like to use the Music library with some other VS1053b-based board that uses different pins.

After initialization, you can call the following methods:

* `Music.play(File& file)` starts playing a music file (and returns immediately). The argument is an open `File` object from Arduino's standard [SD](http://arduino.cc/en/Reference/SD) library.

* `Music.cancel()` cancels playback.

* `Music.loop()` needs to be called over and over again and does all the actual work, which basically amounts to keeping the VS1053b's buffer filled with data from the music file. If you don't call `loop()` frequently enough, you'll probably get distorted or skipping sound. (See below for what "frequently enough" means.)

* `Music.volume(uint8_t vol)` sets the playback volume on a linear scale from 0 (completely silent) to 255 (as loud as possible). The default is maximum volume. There are actually just 32 distinct volume levels available. Calling just `volume()`, without any arguments, returns the current volume.

* `Music.balance(int8_t bal)` sets the balance between the left and right channels. -128 is all to the left (right channel is silent), +127 is all to the right (left channel silent), and 0, which is also the default, means both channels are at the same volume. Calling just `balance()`, without any arguments, returns the current balance.

* `Music.state()` returns one of the following values:
  * `MUSIC_STATE_IDLE` means that the library is currently not doing anything at all, and is ready to play music.  `play()` can only be called in this state. (It will be silently ignored in any other state.) It is safe (and efficient), but not necessary, to keep calling `loop()` in this state.
  * `MUSIC_STATE_PLAYING` means that the library is currently playing a music file.
  * `MUSIC_STATE_BUSY` means that the library is currently busy flushing the VS1053b chip's buffer after playback ended (because the end of the music file was reached or because you called `cancel()`). This state shouldn't last long, but you absolutely need to keep calling `loop()` at least until the library is back in idle state.

* `Music.reset()` does a hardware and software reset of the VS1053b chip. This is done automatically when `begin()` is called and really shouldn't be necessary during normal operation. When the VS1053b chip resets, you'll probably hear a soft clicking sound in the attached speakers; that's when the built-in DAC is switched on.

The VS1053b chip has 2048 bytes of internal buffer. Depending on your music file's bit rate, that should give you ample time between consecutive `loop()` invocations to do your other stuff  for reference, a full buffer's worth of a 128 kbps MP3 file amounts to a bit more than 100 milliseconds that you are free to use as you please while the VS1053b chip runs out of data.

You'll get best results if you call `loop()` as frequently as you can, and if your other code is also done in a non-blocking manner  see the [BlinkWithoutDelay](http://arduino.cc/en/Tutorial/BlinkWithoutDelay) tutorial for an example of how to do that.



It's better!
------------

It's non-blocking! (So your Arduino is free to do other stuff in parallel to playing music.)

The Music library depends on Arduino's standard [SD](http://arduino.cc/en/Reference/SD) and [SPI](http://arduino.cc/en/Reference/SPI) libraries, and you can use them in your own code as well, if you need. For example, there's no reason why you couldn't or should't use the SD card to store your own non-music files, too. And the [SD](http://arduino.cc/en/Reference/SD) library supports subdirectories on the SD card, while Seeedstudio's library requires all files to be in the root directory.

The per-channel volume can be manipulated as a combination of a linear volume and a balance setting, which seems a lot more practical than the non-linear semi-decibels of attenuation per channel exposed by the VS1053b chip itself.

There are a few bugs in Seeedstudio's library that, for one reason or another, aren't in the Music library:

* As long as the VS1053b chip's clock frequency multiplier hasn't been set to 3.5x, talking to it over SPI with full speed can cause it to receive garbled data  and that can cause the chip to not be initialized properly, which in turn can lead to distorted playback (and flaky SPI communication in general). Seeedstudio's code puts in a rather long delay after the initial SPI communication to set the clock frequency multiplier to work around that, but that's actually not necessary: Simply reduce the SPI clock speed during that early communication and upgrade the speed once the VS1053b chip is running at speed.

* Pressing one of the on-board buttons blocks the library's internal loop until the button is released, which could conceivably lead to buffer underflow in the VS1053b chip's audio buffer. (Since most button presses skip or start or stop music anyway, that's only an issue when changing the volume, I guess.)



It's worse!
-----------

There is no playlist support. And you'll have to open your music files yourself.

The Music library does nothing with the on-board LED and buttons on Seeedstudio's Music Shield  but those are trivial to use in your own code, if you want to. (Or you can ignore them and use those pins otherwise, at least those connected to the buttons.)

The SD and SPI libraries used by Seeedstudio's library are slimmed down, which saves some program memory space.


=====


(C) 2013 Michael Buschbeck (michael@buschbeck.net)

This work is licensed under a Creative Commons 3.0 Unported License and distributed in the hope that it will be useful, but without any warranty. See http://creativecommons.org/licenses/by/3.0/ for details.
