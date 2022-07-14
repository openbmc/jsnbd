# Virtual Media Service

## Overview

This component allows users to remotely mount ISO/IMG drive images through BMC
to Server HOST. The Remote drive is visible in HOST as USB storage device. This
can be used to access regular files or even be used to install OS on bare metal
system.

## Glossary

- **Proxy mode** - Redirection mode that works directly from browser and uses
  JS/HTML5 to communicate over Secure Websockets directly to HTTPs endpoint
  hosted on BMC.
- **Standard mode** - Redirection is initiated from browser using Redfish
  defined Virtual Media schemas, BMC connects to an external CIFS/HTTPs image
  pointed during initialization.
- **NBD** - Network Block Device.
- **USB Gadget** - Part of Linux kernel that makes emulation of certain USB
  device classes possible through USB "On-The-Go".

## Capabilities

This application is implementation of Virtual Media proposed in OpenBMC design
docs `[1]`.

- Defines DBus API in order to expose configuration and do actions on the
  service.
- Spawns nbd-client instances and monitors their lifetime for proxy connections.
- Spawns nbdkit instances and monitors their lifetime for CIFS/HTTPS
  connections.
- Monitors udev for all NBD-related block device changes and configures/
  deconfigures USB Gadget accordingly.
- Supports multiple and simultaneous connections in both standard and proxy
  mode.

## How to build

### System/runtime dependencies

In order to allow Virtual Media service work some dependencies are needed
running:

- nbd-client (kernel support with nbd-client installed) `[2]`
- NBDKit (part of libguestfs) `[3]`
- USB Gadget (enabled in kernel) `[4]`
- samba (kernel part, must be enabled) `[5]`

### Compilation dependencies

- **Meson** >= 1.1.1 (https://mesonbuild.com)

- **libudev-dev**

  Udev development packages are needed to perform polling of ndb block device
  properties

- **boost** >= 1.80

  Boost provides free peer-reviewed portable C++ source libraries
  (https://www.boost.org)

- **nlohmann-json**

  Nlohmann-Json is a JSON manipulation library written in modern C++
  (https://github.com/nlohmann/json)

- **sdbusplus**

  C++ library for interacting with D-Bus, built on top of the sd-bus library
  from systemd (https://github.com/openbmc/sdbusplus)

**Note:** When not found in the environment, Meson will try to download and
configure missing packages.

### Build procedure

Virtual Media is compiled with use of standard Meson flow:

```bash
> meson build
> cd build
> ninja
```

#### Compilation options

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
