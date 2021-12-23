#pragma once

#include <sdbusplus/asio/object_server.hpp>

#ifdef INJECT_MOCKS
#include "mocks/object_server_mock.hpp"
using object_server = virtual_media_mock::object_server_mock;
#else
using object_server = sdbusplus::asio::object_server;
#endif
