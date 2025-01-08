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

    Mounter(const Mounter&) = delete;
    Mounter(Mounter&&) = delete;

    Mounter& operator=(const Mounter&) = delete;
    Mounter& operator=(Mounter&&) = delete;

  private:
    std::string target;
};

} // namespace utils
