

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

// displayCallback called instead of infoboxCallback
template <typename T>
FLASHMEM void Property<T>::Init(T val, FuncPtr _fPtr) {
  value = val;
  displayCallback = _fPtr;
}

// displayCallback called instead of infoboxCallback
template <typename T>
FLASHMEM void Property<T>::Init(T val, FuncPtr _fPtr, int _id, bool polled/* = true */) {
  value = val;
  displayCallback = _fPtr;
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
// displayCallback called instead of infoboxCallback
template <typename T>
FLASHMEM void Property<T>::Init(T val, T _min, T _max, bool circ, FuncPtr _fPtr, int _id, bool polled/* = true */) {
  value = val;
  hasMinMax = true;
  min = _min;
  max = _max;
  minmaxCircular = circ;
  displayCallback = _fPtr;
  id = _id;
  notifyOnPoll = polled;
}

// with bounds check int (*boundsCallback)(T)
// displayCallback called instead of  infoboxCallback
template <typename T>
FLASHMEM void Property<T>::Init(T val, BoundPtr _bPtrInt, FuncPtr _fPtr, int _id, bool polled/* = true */) {
  value = val;
  hasMinMax = true;
  boundsCallback = _bPtrInt;
  displayCallback = _fPtr;
  id = _id;
  notifyOnPoll = polled;
}

template class Property<int>;
template class Property<float>;
template class Property<unsigned long>;
