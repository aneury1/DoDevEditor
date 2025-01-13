#include "sdk.h"
#include <iostream>
#include <sstream>

#define LOG_I(msg) \
{ \
  std::stringstream stream;\
  stream << __FILE__ <<":"<<__LINE__<<": "<<msg<<"\n"; \
  std::cout << stream.str(); \
}