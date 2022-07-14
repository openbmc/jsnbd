# nbd-proxy

Prototype javascript+websocket NBD server; this code demonstrates a javascript
NBD implementation connected to the kernel nbd device over a websocket.

There are two components here:

nbd-proxy: a little binary to initialise a nbd client, connected to a unix
domain socket, then proxy data between that socket and stdio. This can be used
with a websocket proxy to expose that stdio as a websocket.

nbd.js: a javascript implementation of a NBD server.

## Running

You'll need a websocket proxy This connects the nbd-proxy component to a
websocket endpoint.

For experimentation, I use the `websocketd` infrastrcture to expose the
websocket endpoint, plus serve the static HTML+js client:

    git clone https://github.com/joewalnes/websocketd
    (cd websocketd && make)

    sudo websocketd/websocketd --port=8000 --staticdir=web --binary ./nbd-proxy <config>

- where `<config>` is a name of a configuration in the config.json file.

Note that this type of invocation is very insecure, and intended just for
experimentation. See the Security section below.

For real deployments, you want your websocket-enabled service to run nbd-proxy,
and connect its stdio to a websocket, running in binary mode. Your web interface
will interact with this using an instance of the NBDServer object (defined in
web/js/nbd.js):

    var server = NBDServer(endpoint, file);
    server.start();

- where endpoint is the websocket URL (ws://...) and file is a File object. See
  web/index.html for an example.

## Security

This code allows potentially-untrusted clients to export arbitrary block device
data to your kernel. Therefore, you should ensure that only trusted clients can
connect as NBD servers.

There is no authentication or authorisation implemented in the nbd proxy. Your
websocket proxy should implement proper authentication before nbd-proxy is
connected to the websocket endpoint.

## State hook

The nbd-proxy has a facility to run an program on state change. When a nbd
session is established or shut down, the proxy will run the executable at
/etc/nbd-proxy/state.

This executable is called with two arguments: the action ("start" or "stop"),
and the name of the configuration (as specified in the config.json file).

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
