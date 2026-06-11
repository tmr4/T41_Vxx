
#include "connectBase.h"

template class AudioConnectBuffered<size_t, volatile size_t>;
template class AudioConnectBuffered<volatile size_t, size_t>;
