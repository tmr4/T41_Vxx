#pragma once

#include <Arduino.h>

#include "catControl.h"

class T41Update {
  typedef void (*UpdateCallback)(int);

public:
  T41Update(uint16_t token) : catToken(token), catHash(CatToken2Hash(token)) {}

  //uint16_t getCatToken() const { return catToken; }
  //uint8_t getCatHash() const { return catHash; }

  //virtual void setValue(uint32_t newValue) = 0;
  virtual int getValue() = 0;

  // *** TODO: revisit when infobox is modernized ***
  template<class T, void (T::*Method)(int)>
  static void SetUpdateFunctions(UpdateCallback ib, T* instance) {
    static T* obj = instance; // specific instance of T (static for class)

    infoboxCallback = ib;

    // lambda with no captures [] converted to a raw function pointer
    // We use 'obj' (the static pointer) inside to provide the context.
    remoteCallback = [](int val) {
      (obj->*Method)(val);
    };
  }
  /*
  future consideration:
  template<class T, void (T::*Method)(int)>
  static void AddUpdateFunction(int slot, T* instance) {
      static T* obj = instance;
      _callbacks[slot] = [](int val) { (obj->*Method)(val); };
  }
  */

protected:
  static inline UpdateCallback infoboxCallback = nullptr;
  static inline UpdateCallback remoteCallback = nullptr;

  uint16_t catToken = 0;
  uint8_t catHash = 0;
};

template<typename T>
class ReadOnlyProperty : public T41Update {
public:
  ReadOnlyProperty(T val) : T41Update(0), value(val) {}
  ReadOnlyProperty(T val, uint16_t token) : T41Update(token), value(val) {}

  operator T() { return value; }

  //void setValue(uint32_t newValue) override { }
  int getValue() override { return (int)value; }

protected:
  T value;
};

template<typename T>
class Property : public ReadOnlyProperty<T> {
  typedef void (*FuncPtr)();
  typedef void (*FuncPtrT)(T);
  typedef void (*FuncPtr2Int)(int, int);
  typedef int (*BoundPtr)(int);

protected:
  //using ReadOnlyProperty<T>::value;

public:
  using ReadOnlyProperty<T>::value;
  using ReadOnlyProperty<T>::operator T; // *** this isn't useful, see below ***

  Property(T val) : ReadOnlyProperty<T>(val) {}
  Property(T val, uint16_t token) : ReadOnlyProperty<T>(val, token) {}

  void Init(T val);

  // displayCallback called instead of infoboxCallback
  void Init(T val, FuncPtr _fPtr);

  // displayCallback called instead of  infoboxCallback
  void Init(T val, FuncPtr _fPtr, int _id, bool polled = true);

  // w/ min/max
  // calls to T41Update
  void Init(T val, T _min, T _max, bool circ, int _id, bool polled = true);

  // w/ min/max
  // displayCallback called instead of infoboxCallback
  void Init(T val, T _min, T _max, bool circ, FuncPtr _fPtr, int _id, bool polled = true);

  // with bounds check int (*bPtrInt)(T)
  // displayCallback called instead of  infoboxCallback
  void Init(T val, BoundPtr _bPtrInt, FuncPtr _fPtr, int _id, bool polled = true);

  bool Poll(bool updateDisplay, bool updateRemote, bool override = false) {
    bool tmp = hasChanged;

    if(override) {
      Notify();
    } else if((hasChanged && notifyOnPoll)) {
      //Serial.printf("Both update: id: %d, value: %d, display: %d, remote: %d\n", id, value, updateDisplay, updateRemote);
      if(updateDisplay) UpdateDisplay();
      if(updateRemote) UpdateRemote();
    } else if(updated) {
      if(updateDisplay) UpdateDisplay();
      //Serial.printf("Display update: id: %d, value: %d, display: %d\n", id, value, updateDisplay);
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
  using ReadOnlyProperty<T>::catToken;
  using ReadOnlyProperty<T>::remoteCallback;

  void Notify() {
    UpdateDisplay();
    UpdateRemote();
  }

  void UpdateRemote() {
    //if((T41Update::remoteCallback != nullptr) && (id >= 0)) (*T41Update::remoteCallback)(id);
    //if((T41Update::remoteCallback != nullptr) && (id >= 0)) (*T41Update::remoteCallback)((int)catToken);
    if((remoteCallback != nullptr) && (catToken > 0)) remoteCallback((int)catToken);
  }

  void UpdateDisplay() {
    if(displayCallback != nullptr) {
      (*displayCallback)();
    } else if((T41Update::infoboxCallback != nullptr) && (id >= 0)) {
      (*T41Update::infoboxCallback)(id);
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
      if(bPtrInt != nullptr) {
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
    if(tmp != value) {
      hasChanged = true;
      if(!notifyOnPoll) {
        Notify();
      }
      //Serial.printf("id: %d, value: %d, tmp: %d, hasChanged: %d\n", id, value, tmp, hasChanged);
    }

    return value;
  }

  FuncPtr displayCallback = nullptr;
  BoundPtr bPtrInt = nullptr;

public:
  //void setValue(uint32_t newValue) override { Update((T)newValue); }
};
