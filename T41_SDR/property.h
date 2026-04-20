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

  FLASHMEM void Init(T val, FuncPtr npc = NULL) {
    value = val;
    for(int i = 0; i < 5; i++) {
      NotifyPropertyChanged[i] = NULL;
    }
    notify = 0;
    if(npc != NULL) {
      NotifyPropertyChanged[notify++] = npc;
    }
  }

  FLASHMEM void InitID(T val, int _min, int _max, FuncPtrID npc, int _id, bool polled = true) {
    value = val;
    min = _min;
    max = _max;
    NotifyPropertyChangedID = npc;
    id = _id;
    notifyOnPoll = polled;
  }

  FLASHMEM void AddNotify(FuncPtr npc = NULL) {
    if(++notify >= 5) notify = 0; // just overwrite starting with oldest
    if(npc != NULL) {
      NotifyPropertyChanged[notify++] = npc;
    }
  }

  FLASHMEM void NotifyOnPoll(bool val) {
    notifyOnPoll = val;
    hasChanged = false;
  }

  FLASHMEM bool Poll(bool override = false) {
    bool tmp = hasChanged;

    if((hasChanged && notifyOnPoll) || override) {
      Notify();
    }
    hasChanged = false;
    return tmp;
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
    for(int i = 0; i < notify; i++) {
      // *** shouldn't need this check ***
      if(NotifyPropertyChanged[i] != NULL) {
        (*NotifyPropertyChanged[i])(value);
      }
    }
    if(NotifyPropertyChangedID != NULL && id >= 0) {
      (*NotifyPropertyChangedID)(id);
    }
  }

private:
  T value;
  int min, max;

  int notify = 0;
  bool hasChanged = false;
  bool notifyOnPoll = false;

  int id;

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

  FuncPtr NotifyPropertyChanged[5] = { NULL };
  FuncPtrID NotifyPropertyChangedID = NULL;
};
