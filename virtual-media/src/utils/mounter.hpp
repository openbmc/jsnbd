#pragma once

#include <string>

namespace utils
{

class Mounter
{
  public:
    explicit Mounter(const std::string& source, const std::string& target,
                     const std::string& filesystemtype,
                     unsigned long mountflags, const void* data);
    ~Mounter();

  private:
    std::string target;
};

} // namespace utils
