#pragma once

#include "utils/pipe_reader.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/process.hpp>

#include <string>
#include <vector>

namespace utils
{

class Child
{
  public:
    Child() = default;
    Child(boost::asio::io_context& ioc, const std::string& app,
          const std::vector<std::string>& args, PipeReader& reader);

    int exitCode();
    int nativeExitCode();
    bool running();
    void terminate();
    void wait();

  private:
    boost::process::child child;
};

} // namespace utils
