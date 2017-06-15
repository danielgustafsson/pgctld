# pgctld

A simple proof-of-concept hack wrapping `pg_ctl` in a https API. This requires
[kore.io](https://kore.io) installed. To test locally, edit the hardcoded paths
in `src/pgctld.c` and build/run:

    kodev build
    kodev run

This a hack to test out using kore for this, and as such it's incomplete and
not to be used for anything real. Currently, `GET /status` and `GET /start` are
supported. Restarting the API server right now blocks on the forked postgres
process keeping the socket bound, this is a known limitation which will be
fixed.

To invoke, run the API server and call it with curl:

    :~ $ curl -k https://127.0.0.1:8888/status
    pg_ctl: no server running
    :~ $ curl -k https://127.0.0.1:8888/start
    ^C
    :~ $ curl -k https://127.0.0.1:8888/status
    pg_ctl: server is running (PID: 40167)

Currently there is no postback from `/start` to report status, this too will be
fixed.

The kore installation must be built using the `TASKS=1` flavor.
