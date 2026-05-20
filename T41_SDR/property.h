#pragma once

#include <Arduino.h>

template<typename T>
class ReadOnlyProperty {
public:
  ReadOnlyProperty() {}

  operator T() { return value; }

  void Set(T val) {
    value = val;
  }

protected:

private:
  T value;
};

class T41Update {
  typedef void (*FuncPtrInt)(int);
  //typedef void (*FuncPtr2Int)(int, int);

public:
  T41Update() {}

  static void SetUpdateFunctions(FuncPtrInt ib, FuncPtrInt rm) {
    fPtrInfoBox = ib;
    fPtrRemote = rm;
  }

protected:
  static inline FuncPtrInt fPtrInfoBox = NULL;
  static inline FuncPtrInt fPtrRemote = NULL;

private:
};

// *** FLASHMEM made no difference in code placement, couldn't verify placement in listing file  ***
template<typename T>
class Property : public T41Update {
  typedef void (*FuncPtr)();
  typedef void (*FuncPtrT)(T);
  typedef void (*FuncPtrInt)(int);
  typedef void (*FuncPtr2Int)(int, int);
  typedef int (*BoundPtr)(int);

public:
  FLASHMEM Property() {}

  FLASHMEM void Init(T val) {
    value = val;
  }

  // fPtr called instead of fPtrInfoBox
  FLASHMEM void Init(T val, FuncPtr _fPtr) {
    value = val;
    fPtr = _fPtr;
  }

  // fPtr called instead of  fPtrInfoBox
  FLASHMEM void Init(T val, FuncPtr _fPtr, int _id, bool polled = true) {
    value = val;
    fPtr = _fPtr;
    id = _id;
    notifyOnPoll = polled;
  }

  // w/ min/max
  // calls to T41Update
  FLASHMEM void Init(T val, T _min, T _max, bool circ, int _id, bool polled = true) {
    value = val;
    hasMinMax = true;
    min = _min;
    max = _max;
    minmaxCircular = circ;
    id = _id;
    notifyOnPoll = polled;
  }

  // w/ min/max
  // fPtr called instead of fPtrInfoBox
  FLASHMEM void Init(T val, T _min, T _max, bool circ, FuncPtr _fPtr, int _id, bool polled = true) {
    value = val;
    hasMinMax = true;
    min = _min;
    max = _max;
    minmaxCircular = circ;
    fPtr = _fPtr;
    id = _id;
    notifyOnPoll = polled;
  }

  // with bounds check int (*bPtrInt)(T)
  // fPtr called instead of  fPtrInfoBox
  FLASHMEM void Init(T val, BoundPtr _bPtrInt, FuncPtr _fPtr, int _id, bool polled = true) {
    value = val;
    hasMinMax = true;
    bPtrInt = _bPtrInt;
    fPtr = _fPtr;
    id = _id;
    notifyOnPoll = polled;
  }

  FLASHMEM bool Poll(bool updateDisplay, bool updateRemote, bool override = false) {
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
  FLASHMEM void Update(T val) {
    value = val;
    updated = true;
  }

  //operator T() { return get(); }
  operator T() { return value; }
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
  FLASHMEM void Notify() {
    UpdateDisplay();
    UpdateRemote();
  }

  FLASHMEM void UpdateRemote() {
    if((T41Update::fPtrRemote != NULL) && (id >= 0)) (*T41Update::fPtrRemote)(id);
  }

  FLASHMEM void UpdateDisplay() {
    if(fPtr != NULL) {
      (*fPtr)();
    } else if((T41Update::fPtrInfoBox != NULL) && (id >= 0)) {
      (*T41Update::fPtrInfoBox)(id);
    }
  }

private:
  T value;
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
};
