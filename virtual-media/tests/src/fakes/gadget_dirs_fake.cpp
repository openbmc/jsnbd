#include "utils/gadget_dirs.hpp"

#include <string>

namespace utils
{

std::string GadgetDirs::gadgetPrefix()
{
    return {"tests/mass-storage-"};
}

std::string GadgetDirs::busPrefix()
{
    return {"tests/bus"};
}

} // namespace utils
