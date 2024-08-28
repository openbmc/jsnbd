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

- where <config> is a name of a configuration in the config.json file.

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
