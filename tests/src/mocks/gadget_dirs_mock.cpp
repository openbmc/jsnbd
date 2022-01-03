#include "utils/gadget_dirs.hpp"

namespace utils
{

const std::string GadgetDirs::gadgetPrefix()
{
    return {"tests/mass-storage-"};
}

const std::string GadgetDirs::busPrefix()
{
    return {"tests/bus"};
}

} // namespace utils
