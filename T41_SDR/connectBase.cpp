
#include "connectBase.h"

template class ConnectBuffered<size_t, volatile size_t>;
template class ConnectBuffered<volatile size_t, size_t>;
