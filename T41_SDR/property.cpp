

#include "property.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

template <typename T>
FLASHMEM void Property<T>::Init(T val) {
  value = val;
}

// fPtr called instead of fPtrInfoBox
template <typename T>
FLASHMEM void Property<T>::Init(T val, FuncPtr _fPtr) {
  value = val;
  fPtr = _fPtr;
}

// fPtr called instead of  fPtrInfoBox
template <typename T>
FLASHMEM void Property<T>::Init(T val, FuncPtr _fPtr, int _id, bool polled/* = true */) {
  value = val;
  fPtr = _fPtr;
  id = _id;
  notifyOnPoll = polled;
}

// w/ min/max
// calls to T41Update
template <typename T>
FLASHMEM void Property<T>::Init(T val, T _min, T _max, bool circ, int _id, bool polled/* = true */) {
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
template <typename T>
FLASHMEM void Property<T>::Init(T val, T _min, T _max, bool circ, FuncPtr _fPtr, int _id, bool polled/* = true */) {
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
template <typename T>
FLASHMEM void Property<T>::Init(T val, BoundPtr _bPtrInt, FuncPtr _fPtr, int _id, bool polled/* = true */) {
  value = val;
  hasMinMax = true;
  bPtrInt = _bPtrInt;
  fPtr = _fPtr;
  id = _id;
  notifyOnPoll = polled;
}

template class Property<int>;
template class Property<float>;
template class Property<unsigned long>;
