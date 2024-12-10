#include "utils/gadget_dirs.hpp"

#include <string>

namespace utils
{

std::string GadgetDirs::gadgetPrefix()
{
    return {"/sys/kernel/config/usb_gadget/mass-storage-"};
}

std::string GadgetDirs::busPrefix()
{
    return {"/sys/bus/platform/devices/1e6a0000.usb-vhub"};
}

} // namespace utils
