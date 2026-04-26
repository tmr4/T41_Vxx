#pragma once

#include <Arduino.h>

// *** FLASHMEM made no difference in code placement, couldn't verify placement in listing file  ***
template<typename T>
class Property {
  typedef void (*FuncPtr)();
  typedef void (*FuncPtrT)(T);
  typedef void (*FuncPtrInt)(int);
  typedef int (*BoundPtr)(int);

public:
  FLASHMEM Property(FuncPtrT npc = NULL) {
    id = -1;
  }

  FLASHMEM void Init(T val) {
    value = val;
  }

  //FLASHMEM void Init(T val, FuncPtrT npc = NULL) {
  //  value = val;
  //  for(int i = 0; i < 5; i++) {
  //    NotifyChanged[i] = NULL;
  //  }
  //  notify = 0;
  //  if(npc != NULL) {
  //    NotifyChanged[notify++] = npc;
  //  }
  //}

  // initialize property with 1 notification (set to NULL if not needed):
  // not polled
  // notify: void (*funcPtr)()
  FLASHMEM void Init(T val, FuncPtr _fPtr) {
    value = val;
    fPtr = _fPtr;
  }

  // initialize property with 2 notifications (set either to NULL if not needed):
  // both polled unless specified otherwise
  //   notify: void (*funcPtrT)(T) with copy of property
  //   notify: void (*funcPtr)()
  FLASHMEM void Init(T val, FuncPtrT _fPtrT, FuncPtr _fPtr, bool polled = true) {
    value = val;
    fPtrT = _fPtrT;
    fPtr = _fPtr;
    notifyOnPoll = polled;
  }

  // same as above with max, min
  FLASHMEM void Init(T val, T _min, T _max, bool circ, FuncPtrT _fPtrT, FuncPtrInt _fPtrInt, int _id, bool polled = true) {
    value = val;
    hasMinMax = true;
    min = _min;
    max = _max;
    minmaxCircular = circ;
    fPtrT = _fPtrT;
    fPtrInt = _fPtrInt;
    id = _id;
    notifyOnPoll = polled;
  }

  // same as above with max, min
  FLASHMEM void Init(T val, T _min, T _max, bool circ, FuncPtrT _fPtrT, FuncPtr _fPtr, bool polled = true) {
    value = val;
    hasMinMax = true;
    min = _min;
    max = _max;
    minmaxCircular = circ;
    fPtrT = _fPtrT;
    fPtr = _fPtr;
    notifyOnPoll = polled;
  }

  // same as above with bounds check int (*bPtrInt)(T)
  FLASHMEM void Init(T val, BoundPtr _bPtrInt, FuncPtrT _fPtrT, FuncPtr _fPtr, bool polled = true) {
    value = val;
    hasMinMax = true;
    bPtrInt = _bPtrInt;
    fPtrT = _fPtrT;
    fPtr = _fPtr;
    notifyOnPoll = polled;
  }

  //FLASHMEM void AddNotify(FuncPtrT npc = NULL) {
  //  if(++notify >= 5) notify = 0; // just overwrite starting with oldest
  //  if(npc != NULL) {
  //    NotifyChanged[notify++] = npc;
  //  }
  //}

  //FLASHMEM void NotifyOnPoll(bool val) {
  //  notifyOnPoll = val;
  //  hasChanged = false;
  //}

  FLASHMEM bool Poll(bool updateDisplay, bool updateRemote, bool override = false) {
    bool tmp = hasChanged;

    if(override) {
      Notify();
    } else if((hasChanged && notifyOnPoll)) {
      if(updateDisplay) UpdateDisplay();
      if(updateRemote && fPtrT != NULL) (*fPtrT)(value);
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

  operator T() { return get(); }
  const T& operator=(const T& val) { return set(val); }
  const T& operator+=(const T& val) { return set(value + val); }
  const T& operator-=(const T& val) { return set(value - val); }
  const T& operator++() { return set(++value); }
  const T operator++(int) {
    T tmp = value;
    set(++value);
    return tmp;
  }

protected:
  FLASHMEM void Notify() {
    UpdateDisplay();
    if(fPtrT != NULL) (*fPtrT)(value);
  }

  FLASHMEM void UpdateDisplay() {
    if(fPtr != NULL) {
      (*fPtr)();
    }
    if((fPtrInt != NULL) && (id >= 0)) {
      (*fPtrInt)(id);
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

  T get() { return value; }

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
  FuncPtrT fPtrT = NULL;
  FuncPtrInt fPtrInt = NULL;
  BoundPtr bPtrInt = NULL;
};
