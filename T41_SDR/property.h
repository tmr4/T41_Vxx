#pragma once

#include <Arduino.h>

// *** FLASHMEM made no difference in code placement, couldn't verify placement in listing file  ***
template<typename T>
class Property {
  typedef void (*FuncPtr)(T);
  typedef void (*FuncPtrID)(int);

public:
  FLASHMEM Property(FuncPtr npc = NULL) {
    id = -1;
  }

  FLASHMEM void Init(T val) {
    value = val;
  }

  //FLASHMEM void Init(T val, FuncPtr npc = NULL) {
  //  value = val;
  //  for(int i = 0; i < 5; i++) {
  //    NotifyChanged[i] = NULL;
  //  }
  //  notify = 0;
  //  if(npc != NULL) {
  //    NotifyChanged[notify++] = npc;
  //  }
  //}

  FLASHMEM void Init(T val, T _min, T _max, FuncPtr nr, FuncPtrID nd, int _id, bool polled = true) {
    value = val;
    min = _min;
    max = _max;
    NotifyRemote = nr;
    NotifyDisplay = nd;
    id = _id;
    notifyOnPoll = polled;
  }

  //FLASHMEM void AddNotify(FuncPtr npc = NULL) {
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
      if(updateDisplay) (*NotifyDisplay)(id);
      if(updateRemote) (*NotifyRemote)(value);
    } else if(updated) {
      if(updateDisplay) (*NotifyDisplay)(id);
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
    //for(int i = 0; i < notify; i++) {
    //  // *** shouldn't need this check ***
    //  if(NotifyChanged[i] != NULL) {
    //    (*NotifyChanged[i])(value);
    //  }
    //}
    if(NotifyDisplay != NULL && id >= 0) {
      (*NotifyDisplay)(id);
    }
  }

private:
  T value;
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
    if(id >= 0) {
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

  //FuncPtr NotifyChanged[5] = { NULL };
  FuncPtr NotifyRemote = NULL;
  FuncPtrID NotifyDisplay = NULL;
};
