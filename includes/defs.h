#ifndef DEFS_H_DEFINED
#define DEFS_H_DEFINED
#include <cstdint>
#include <string>
#include <vector>

enum class ResponseStatus : uint32_t{
   Ok,
   Success,
   Error,
   Failed,
   Disconnected,
   Unexpected,
   NotAvailable,
   NotExistent,
   Invalid
};

typedef std::string String;
typedef std::vector<uint8_t> ByteBuffer;



#endif // DEFS_H_DEFINED