#pragma once

namespace utils
{

class Mounter
{
  public:
    explicit Mounter(const char* source, const char* target,
                     const char* filesystemtype, unsigned long mountflags,
                     const void* data);
    ~Mounter();

  private:
    const char* target;
};

} // namespace utils
