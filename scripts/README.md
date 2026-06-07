The `throttle.sh` script can be used to manipulate the bandwidth (i.e., speed and ping latency).
We can used this script to mimic the WAN/LAN setting within lab enviorments, e.g., running program within one machine.

## Unit tests

`run-tests.sh` runs all SCI 2-party unit tests (conv-HE and elemwise_prod-HE were commented out,
since they use headers that were no longer present in the project prior to the refactorization)
built into `build/bin/` as ALICE (`r=1`) + BOB (`r=2`) pairs on the same machine.
Each pair prints PASS or FAIL. Logs go to `/tmp/<test>.{alice,bob}.log`.

```bash
bash scripts/run-tests.sh
```

The test binaries are produced as part of `bash scripts/build.sh`
(`SCI_BUILD_TESTS=ON` is on by default).

## Orchestration

`run-orchestrator.sh` starts the orchestrator and writes node
metrics and routing decisions under `results/orchestration/` by default.

```bash
bash scripts/run-orchestrator.sh [round_robin|least_connections|energy_aware]
```

`run-orchestration-server.sh` starts a multi-client SNNI server and a
server-side `node-reporter` for that server.

```bash
ORCHESTRATOR_IP=<orchestrator-host> bash scripts/run-orchestration-server.sh [cheetah|SCI_HE] [server_ip]
```

`run-orchestration-client.sh` optionally starts a client-side reporter,
then runs `build/bin/client` through the orchestrator. Set `RUN_CLIENT_REPORTER=0` to skip the client reporter.

```bash
bash scripts/run-orchestration-client.sh [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [orchestrator_ip]
```

## Multi-client server

`run-server-multi.sh` starts the long-running multi-client server with Cheetah or SCI-HE backend.
It loads all three networks' weights and listens on `$SERVER_PORT` (12345 by default in `common.sh`).
Note: Server stays up until killed.

```bash
bash scripts/run-server-multi.sh [cheetah|SCI_HE]
```

Build the server binaries once with:

```bash
cmake --build build -j --target server-cheetah server-SCI_HE
```

## Multi-client client

`run-client-multi.sh` performs the 20-byte control-port handshake against a running server,
then runs the matching standalone client on the data port the server assigns to it.
The optional 3rd argument overrides `SERVER_IP` from `common.sh` - pass the server's hostname or IP when client and server are on different machines.

```bash
bash scripts/run-client-multi.sh [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [server_host]
```

Requires `python3` for the handshake's binary pack/unpack.

### Concurrent clients

Each client gets its own data port range from the handshake, so several clients can run against the same server simultaneously
without any manual port assignment - open several terminals on the client machine and invoke the script once per terminal.

```bash
bash scripts/run-client-multi.sh cheetah sqnet       <server-host> > /tmp/c1.log 2>&1 &
bash scripts/run-client-multi.sh cheetah resnet50    <server-host> > /tmp/c2.log 2>&1 &
bash scripts/run-client-multi.sh cheetah densenet121 <server-host> > /tmp/c3.log 2>&1 &
wait
```

The example uses `cheetah` + (`sqnet`, `resnet50`, `densenet121`).
Any of the supported backends (`cheetah`, `SCI_HE`) work, as long as every client uses the same backend as the running server.
For example, 3 SCI-HE clients exercising all three networks against an SCI_HE server look like:

```bash
bash scripts/run-client-multi.sh SCI_HE sqnet       <server-host> > /tmp/c1.log 2>&1 &
bash scripts/run-client-multi.sh SCI_HE resnet50    <server-host> > /tmp/c2.log 2>&1 &
bash scripts/run-client-multi.sh SCI_HE densenet121 <server-host> > /tmp/c3.log 2>&1 &
wait
```

### NOT SUPPORTED
Every client must use the same backend as the running server - a Cheetah client cannot talk to SCI_HE server, and vice versa.