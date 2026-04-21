#pragma once

#include <Arduino.h>

// *** FLASHMEM made no difference in code placement, couldn't verify placement in listing file  ***
template<typename T>
class Property {
  typedef void (*FuncPtr)();
  typedef void (*FuncPtrT)(T);
  typedef void (*FuncPtrInt)(int);

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

  FLASHMEM void Init(T val, FuncPtrInt nd) {
    value = val;
    NotifyDisplay = nd;
  }

  FLASHMEM void Init(T val, T _min, T _max, FuncPtrT nr, FuncPtrInt nd, int _id, bool polled = true) {
    value = val;
    hasMinMax = true;
    min = _min;
    max = _max;
    NotifyRemote = nr;
    NotifyDisplay = nd;
    id = _id;
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
      if(updateRemote && NotifyRemote != NULL) (*NotifyRemote)(value);
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
  }

  FLASHMEM void UpdateDisplay() {
    if(NotifyDisplay != NULL) {
      if(id < 0) {
        (*NotifyDisplay)((int)value);
      } else {
        (*NotifyDisplay)(id);
      }
    }
  }

private:
  T value;
  bool hasMinMax = false;
  T min, max;
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
      if(value > max) value = max;
      if(value < min) value = min;
    }
    if(tmp != val) {
      hasChanged = true;
      if(!notifyOnPoll) {
        Notify();
      }
    }
    return value;
  }

  //FuncPtrT NotifyChanged[5] = { NULL };
  FuncPtrT NotifyRemote = NULL;
  FuncPtrInt NotifyDisplay = NULL;
};
