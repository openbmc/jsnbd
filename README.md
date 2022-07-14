# Sample Virtual Media implementation

## Javascript implementation of a NBD server

web/js/nbd.js: a javascript implementation of a NBD server.

## Virtual Media Service

This component allows users to remotely mount ISO/IMG drive images through BMC
to Server HOST. The Remote drive is visible in HOST as USB storage device. This
can be used to access regular files or even be used to install OS on bare metal
system.

## Glossary

| Key         | Description                                                                                                                                                   |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Proxy mode  | Redirection mode that works directly from browser and uses JS/HTML5 to communicate over Secure Websockets directly to HTTPs endpoint hosted on BMC            |
| Legacy mode | Redirection is initiated from browser using Redfish defined Virtual Media schemas, BMC connects to an external CIFS/HTTPs image pointed during initialization |
| NBD         | Network Block Device                                                                                                                                          |
| USB Gadget  | Part of Linux kernel that makes emulation of certain USB device classes possible through USB "On-The-Go"                                                      |
|             |                                                                                                                                                               |

## Capabilities

This application is implementation of Virtual Media proposed in OpenBMC design
docs `[1]`.

- Allows to redirect images directly from browser or connects directly to an
  external CIFS/HTTPs resource
- Exposes Redfish schema and allows mounting external CIFS/HTTPs images
- Exposes WSS Websocket to initialize Proxy mode connection
- Defines DBus API in order to expose configuration and do actions on the
  service
- Inherits default Redfish and bmcweb authentication and privileges mechanism
- Supports multiple and simultaneous connections in both legacy and proxy mode

## How to build

### System/runtime dependencies

In order to allow Virtual Media service work some dependencies are needed
running:

- nbd-client (kernel support with nbd-client installed) `[2]`
- NBDKit (part of libguestfs) `[3]`
- USB Gadget (enabled in kernel) `[4]`
- samba (kernel part, must be enabled) `[5]`

### Compilation dependencies

- Meson >= 0.63

  [https://https://mesonbuild.com](CMake)

- UDEV devel

  Udev development packages are needed to perform polling of ndb block device
  properties

- boost >= 1.80

  Boost provides free peer-reviewed portable C++ source libraries

  [https://www.boost.org](Boost.org)

  Besides generic boost features, the following libraries has to be enabled:

  - boost_coroutines
  - boost_context

- nlohmann-json

  Nlohmann-Json is a JSON manipulation library written in modern C++

  [Nlohmann-Json library](https://github.com/nlohmann/json)

- systemd devel

  Used for DBus access as sdbusplus dependency

- sdbusplus

  C++ library for interacting with D-Bus, built on top of the sd-bus library
  from systemd

  [https://github.com/openbmc/sdbusplus](Sdbusplus library)

  Note: When not found in the environment sdbusplus and nlohmann will be
  automatically downloaded by CMake.

### Build procedure

VM is compiled with use of standard CMake flow:

```bash
> mkdir build && cd build
> meson build && meson compile -C build
```

#### Compilation options

- `legacy-mode` - (turned on by default) this will enable Legacy mode
- `tests` - (turned on by default) this will build unit test suite
- `verbose-nbdkit-logs` - (turned off by defaultb) in order to debug nbdkit,
  this will add `--verbose` flag when spawning nbdkit instance.

To use any of the above use `meson -Dflag1=enabled -Dflag2=disabled` syntax.

## References

1. [OpenBMC Virtual Media design](https://github.com/openbmc/docs/blob/master/designs/virtual-media.md)
2. [Network Block Device](https://sourceforge.net/projects/nbd/)
3. [nbdkit - toolkit for creating NBD servers](https://libguestfs.org/nbdkit.1.html)
4. [USB Gadget API for Linux](https://www.kernel.org/doc/html/v4.17/driver-api/usb/gadget.html)
5. [Samba - Windows interoperability suite of programs for Linux and Unix](https://www.samba.org/)
