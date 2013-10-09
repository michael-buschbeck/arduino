///////////////////////////////////////////////////////////////////////////////
//
//  Object interface to Arduino pins.
//
//  (C) 2013 Michael Buschbeck <michael@buschbeck.net>
//
//  This work is licensed under a Creative Commons 3.0 Unported License and
//  distributed in the hope that it will be useful, but without any warranty.
//
//  See <http://creativecommons.org/licenses/by/3.0/> for details.
//


#ifndef PIN_H
#define PIN_H

#include <Arduino.h>


////////////////////////////////////////////////////////////////////////////////
//
//  PinDigital<INPUT>
//  PinDigital<OUTPUT>
//

template<uint8_t MODE>
class PinDigital
{
public:
  explicit PinDigital();
  explicit PinDigital(uint8_t address);
  explicit PinDigital(uint8_t address, uint8_t value);

  void begin(uint8_t address);
  void begin(uint8_t address, uint8_t value);

  operator uint8_t ();

  PinDigital& operator = (uint8_t value);

private:
  uint8_t _address;
};


////////////////////////////////////////////////////////////////////////////////
//
//  PinTrigger<RISING>
//  PinTrigger<FALLING>
//  PinTrigger<CHANGE>
//

template<uint8_t CONDITION>
class PinTrigger
{
public:
  explicit PinTrigger();
  explicit PinTrigger(uint8_t address);
  explicit PinTrigger(uint8_t address, uint8_t value);

  void begin(uint8_t address);
  void begin(uint8_t address, uint8_t value);

  operator uint8_t ();

private:
  uint8_t _address;
  uint8_t _state;
};


template<>
class PinTrigger<RISING>
{
public:
  explicit PinTrigger();
  explicit PinTrigger(uint8_t address);
  explicit PinTrigger(uint8_t address, uint8_t value);

  void begin(uint8_t address);
  void begin(uint8_t address, uint8_t value);

  operator uint8_t ();

private:
  uint8_t _address;
  uint8_t _state;
};


template<>
class PinTrigger<FALLING>
{
public:
  explicit PinTrigger();
  explicit PinTrigger(uint8_t address);
  explicit PinTrigger(uint8_t address, uint8_t value);

  void begin(uint8_t address);
  void begin(uint8_t address, uint8_t value);

  operator uint8_t ();

private:
  uint8_t _address;
  uint8_t _state;
};


template<>
class PinTrigger<CHANGE>
{
public:
  explicit PinTrigger();
  explicit PinTrigger(uint8_t address);
  explicit PinTrigger(uint8_t address, uint8_t value);

  void begin(uint8_t address);
  void begin(uint8_t address, uint8_t value);

  operator uint8_t ();

private:
  uint8_t _address;
  uint8_t _state;
};


////////////////////////////////////////////////////////////////////////////////
//
//  PinAnalog<INPUT>
//  PinAnalog<OUTPUT>
//

template<uint8_t MODE>
class PinAnalog;


template<>
class PinAnalog<INPUT>
{
public:
  explicit PinAnalog();
  explicit PinAnalog(uint8_t address);

  void begin(uint8_t address);

  operator int ();

private:
  uint8_t _address;
};


template<>
class PinAnalog<OUTPUT>
{
public:
  explicit PinAnalog();
  explicit PinAnalog(uint8_t address);

  void begin(uint8_t address);

  PinAnalog& operator = (int value);

private:
  uint8_t _address;
};



////////////////////////////////////////////////////////////////////////////////
//
//  PinDigital (implementation)
//

template<uint8_t MODE>
inline PinDigital<MODE>::PinDigital()
  : _address (NOT_A_PIN)
{
  // nothing else to do
}


template<uint8_t MODE>
inline PinDigital<MODE>::PinDigital(uint8_t address)
  : _address (address)
{
  pinMode(_address, MODE);
}


template<uint8_t MODE>
inline PinDigital<MODE>::PinDigital(uint8_t address, uint8_t value)
  : _address (address)
{
  pinMode(_address, MODE);
  digitalWrite(_address, value);
}


template<uint8_t MODE>
inline void PinDigital<MODE>::begin(uint8_t address)
{
  _address = address;
  pinMode(_address, MODE);
}


template<uint8_t MODE>
inline void PinDigital<MODE>::begin(uint8_t address, uint8_t value)
{
  _address = address;
  pinMode(_address, MODE);
  digitalWrite(_address, value);
}


template<uint8_t MODE>
inline PinDigital<MODE>::operator uint8_t ()
{
  return digitalRead(_address);
}


template<uint8_t MODE>
inline PinDigital<MODE>& PinDigital<MODE>::operator = (uint8_t value)
{
  digitalWrite(_address, value);
  return *this;
}


////////////////////////////////////////////////////////////////////////////////
//
//  PinTrigger<RISING> (implementation)
//

inline PinTrigger<RISING>::PinTrigger()
  : _address (NOT_A_PIN)
  , _state   (HIGH)
{
  // nothing else to do
}


inline PinTrigger<RISING>::PinTrigger(uint8_t address)
  : _address (address)
  , _state   (HIGH)
{
  pinMode(_address, INPUT);
}


inline PinTrigger<RISING>::PinTrigger(uint8_t address, uint8_t value)
  : _address (address)
  , _state   (HIGH)
{
  pinMode(_address, INPUT);
  digitalWrite(_address, value);
}


inline void PinTrigger<RISING>::begin(uint8_t address)
{
  _address = address;
  pinMode(_address, INPUT);
}


inline void PinTrigger<RISING>::begin(uint8_t address, uint8_t value)
{
  _address = address;
  pinMode(_address, INPUT);
  digitalWrite(_address, value);
}


inline PinTrigger<RISING>::operator uint8_t ()
{
  uint8_t statePrev = _state;

  _state = digitalRead(_address);

  if (statePrev == LOW && _state == HIGH)
    return RISING;
  
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
//  PinTrigger<FALLING> (implementation)
//

inline PinTrigger<FALLING>::PinTrigger()
  : _address (NOT_A_PIN)
  , _state   (HIGH)
{
  // nothing else to do
}


inline PinTrigger<FALLING>::PinTrigger(uint8_t address)
  : _address (address)
  , _state   (HIGH)
{
  pinMode(_address, INPUT);
}


inline PinTrigger<FALLING>::PinTrigger(uint8_t address, uint8_t value)
  : _address (address)
  , _state   (HIGH)
{
  pinMode(_address, INPUT);
  digitalWrite(_address, value);
}


inline void PinTrigger<FALLING>::begin(uint8_t address)
{
  _address = address;
  pinMode(_address, INPUT);
}


inline void PinTrigger<FALLING>::begin(uint8_t address, uint8_t value)
{
  _address = address;
  pinMode(_address, INPUT);
  digitalWrite(_address, value);
}


inline PinTrigger<FALLING>::operator uint8_t ()
{
  uint8_t statePrev = _state;

  _state = digitalRead(_address);

  if (statePrev == HIGH && _state == LOW)
    return FALLING;
    
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
//  PinTrigger<CHANGE> (implementation)
//

inline PinTrigger<CHANGE>::PinTrigger()
  : _address (NOT_A_PIN)
  , _state   (HIGH)
{
  // nothing else to do
}


inline PinTrigger<CHANGE>::PinTrigger(uint8_t address)
  : _address (address)
  , _state   (HIGH)
{
  pinMode(_address, INPUT);
}


inline PinTrigger<CHANGE>::PinTrigger(uint8_t address, uint8_t value)
  : _address (address)
  , _state   (HIGH)
{
  pinMode(_address, INPUT);
  digitalWrite(_address, value);
}


inline void PinTrigger<CHANGE>::begin(uint8_t address)
{
  _address = address;
  pinMode(_address, INPUT);
}


inline void PinTrigger<CHANGE>::begin(uint8_t address, uint8_t value)
{
  _address = address;
  pinMode(_address, INPUT);
  digitalWrite(_address, value);
}


inline PinTrigger<CHANGE>::operator uint8_t ()
{
  uint8_t statePrev = _state;

  _state = digitalRead(_address);

  if (statePrev != _state)
    return (_state == HIGH ? RISING : FALLING);

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
//  PinAnalog<INPUT> (implementation)
//

inline PinAnalog<INPUT>::PinAnalog()
  : _address (NOT_A_PIN)
{
  // nothing else to do
}


inline PinAnalog<INPUT>::PinAnalog(uint8_t address)
  : _address (address)
{
  pinMode(_address, INPUT);
}


inline void PinAnalog<INPUT>::begin(uint8_t address)
{
  _address = address;
  pinMode(_address, INPUT);
}


inline PinAnalog<INPUT>::operator int ()
{
  return analogRead(_address);
}


////////////////////////////////////////////////////////////////////////////////
//
//  PinAnalog<OUTPUT> (implementation)
//

inline PinAnalog<OUTPUT>::PinAnalog()
  : _address (NOT_A_PIN)
{
  // nothing else to do
}


inline PinAnalog<OUTPUT>::PinAnalog(uint8_t address)
  : _address (address)
{
  pinMode(_address, OUTPUT);
}


inline void PinAnalog<OUTPUT>::begin(uint8_t address)
{
  _address = address;
  pinMode(_address, OUTPUT);
}


inline PinAnalog<OUTPUT>& PinAnalog<OUTPUT>::operator = (int value)
{
  analogWrite(_address, value);
  return *this;
}


#endif
