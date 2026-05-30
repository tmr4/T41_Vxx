#pragma once

#include <Arduino.h>

#include "catControl.h"

class T41Update {
  typedef void (*FuncPtrInt)(int);

public:
  T41Update(uint16_t token) : catToken(token), catHash(CatToken2Hash(token)) {}

  static void SetUpdateFunctions(FuncPtrInt ib, FuncPtrInt rm) {
    fPtrInfoBox = ib;
    fPtrRemote = rm;
  }

  uint16_t getCatToken() const { return catToken; }
  uint8_t getCatHash() const { return catHash; }

  virtual void setFromCAT(uint32_t newValue) = 0;
  virtual uint32_t getForCAT() = 0;

protected:
  static inline FuncPtrInt fPtrInfoBox = NULL;
  static inline FuncPtrInt fPtrRemote = NULL;

//protected:
private:
  uint16_t catToken = 0;
  uint8_t catHash = 0;
};

template<typename T>
class ReadOnlyProperty : public T41Update {
public:
  ReadOnlyProperty(T val) : T41Update(0), value(val) {}
  ReadOnlyProperty(T val, uint16_t token) : T41Update(token), value(val) {}

  operator T() { return value; }

  void setFromCAT(uint32_t newValue) override { }
  uint32_t getForCAT() override { return (uint32_t)value; }

protected:
  T value;
};

template<typename T>
class Property : public ReadOnlyProperty<T> {
  typedef void (*FuncPtr)();
  typedef void (*FuncPtrT)(T);
  typedef void (*FuncPtrInt)(int);
  typedef void (*FuncPtr2Int)(int, int);
  typedef int (*BoundPtr)(int);

protected:
  //using ReadOnlyProperty<T>::value;

public:
  using ReadOnlyProperty<T>::value;
  //using ReadOnlyProperty<T>::operator T; // *** this isn't useful, see below ***

  Property(T val) : ReadOnlyProperty<T>(val) {}
  Property(T val, uint16_t token) : ReadOnlyProperty<T>(val, token) {}

  void Init(T val);

  // fPtr called instead of fPtrInfoBox
  void Init(T val, FuncPtr _fPtr);

  // fPtr called instead of  fPtrInfoBox
  void Init(T val, FuncPtr _fPtr, int _id, bool polled = true);

  // w/ min/max
  // calls to T41Update
  void Init(T val, T _min, T _max, bool circ, int _id, bool polled = true);

  // w/ min/max
  // fPtr called instead of fPtrInfoBox
  void Init(T val, T _min, T _max, bool circ, FuncPtr _fPtr, int _id, bool polled = true);

  // with bounds check int (*bPtrInt)(T)
  // fPtr called instead of  fPtrInfoBox
  void Init(T val, BoundPtr _bPtrInt, FuncPtr _fPtr, int _id, bool polled = true);

  bool Poll(bool updateDisplay, bool updateRemote, bool override = false) {
    bool tmp = hasChanged;

    if(override) {
      Notify();
    } else if((hasChanged && notifyOnPoll)) {
      if(updateDisplay) UpdateDisplay();
      if(updateRemote) UpdateRemote();
    } else if(updated) {
      if(updateDisplay) UpdateDisplay();
    }

    updated = false;
    hasChanged = false;
    return tmp;
  }

  // update property value and display, skip notifications
  void Update(T val) {
    value = val;
    updated = true;
  }

  //operator T() { return this->value; } // *** compiler complains w/o this, even though it's in the base class ***
  //operator T() { return value; }
  const T& operator=(const T& val) { return set(val); }
  const T& operator+=(const T& val) { return set(value + val); }
  const T& operator-=(const T& val) { return set(value - val); }
  // *** TODO: updates and notifications aren't happening properly with ++ or -- (infobox zoom for example w/ mouse) ***
  const T& operator++() { return set(++value); }
  const T operator++(int) {
    T tmp = value;
    set(++value);
    return tmp;
  }
  const T& operator--() { return set(--value); }
  const T operator--(int) {
    T tmp = value;
    set(--value);
    return tmp;
  }

protected:
  void Notify() {
    UpdateDisplay();
    UpdateRemote();
  }

  void UpdateRemote() {
    if((T41Update::fPtrRemote != NULL) && (id >= 0)) (*T41Update::fPtrRemote)(id);
  }

  void UpdateDisplay() {
    if(fPtr != NULL) {
      (*fPtr)();
    } else if((T41Update::fPtrInfoBox != NULL) && (id >= 0)) {
      (*T41Update::fPtrInfoBox)(id);
    }
  }

private:
  bool hasMinMax = false;
  T min, max;
  bool minmaxCircular = false;
  bool hasChanged = false;
  bool updated = false;

  //int notify = 0;
  bool notifyOnPoll = false;

  int id = -1;

  //T get() { return value; }

  const T &set(const T &val) {
    T tmp = value;
    value = val;
    if(hasMinMax) {
      if(bPtrInt != NULL) {
        value = (int)(*bPtrInt)((int)value);
      } else {
        if(minmaxCircular) {
          if(value > max) value = min;
          if(value < min) value = max;
        } else {
          if(value > max) value = max;
          if(value < min) value = min;
        }
      }
    }
    if(tmp != val) {
      hasChanged = true;
      if(!notifyOnPoll) {
        Notify();
      }
    }
    return value;
  }

  FuncPtr fPtr = NULL;
  BoundPtr bPtrInt = NULL;

public:
  void setFromCAT(uint32_t newValue) override { Update((T)newValue); }
};
