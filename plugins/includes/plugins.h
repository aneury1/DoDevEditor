#ifndef PLUGINGS_H_DEFINED
#define PLUGINGS_H_DEFINED
#include <string>
#include <stdint.h>

enum class PluginType : uint32_t{
   Frontend,
   Backend,
   Full,
   Custom
};

struct SimplePlugin{
    virtual PluginType getType() = 0;
    virtual std::string getName() = 0;
    virtual std::string getDescription() = 0;
};




#endif