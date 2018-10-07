# effluence [![Build Status](https://www.travis-ci.com/i-ky/effluence.svg?branch=master)](https://www.travis-ci.com/i-ky/effluence)

[Zabbix](http://www.zabbix.com)
[loadable module](https://www.zabbix.com/documentation/4.0/manual/config/items/loadablemodules)
for real-time export of history to
[InfluxDB](https://www.influxdata.com/time-series-platform/influxdb/)

**Disclaimer**:
As
[the name](https://www.urbandictionary.com/define.php?term=effluence)
suggests,
the module is not ready yet for the use in production,
but testing it is very much appreciated
and any suggestion on performance tuning is welcomed!

## background

From its inception,
using an SQL database for storing both configuration and collected monitoring data
was one of distinctive features of Zabbix.
However,
as Zabbix was gaining acceptance among large enterprises,
the limitations of such approach became evident.
Despite all the optimizations made by Zabbix team over the years,
for large setups performance of the database often becomes a bottleneck,
performance of `insert` queries towards `history*` and `trends*` tables in particular.

Typical ways to cure the problem are:
- partitioning `history*` and `trends*` tables;
- replacing HDDs with SSDs on the database server;
- using clustered drop-in replacements of supported database backends (e.g. [Percona](https://www.percona.com))

But everyone seems to agree that the ultimate solution would be to offload historical data into more suitable storage engine.

Multiple NoSQL solutions and numerous time-series databases were named as alternatives to SQL.
The relevant discussion can be found
[here](https://support.zabbix.com/browse/ZBXNEXT-714).
Since there is no agreement among experts on which exactly NoSQL solution or time-series database Zabbix should use,
there were
[attempts](https://support.zabbix.com/browse/ZBXNEXT-3661)
from Zabbix side to provide a generic interface for historical data storage backend,
leaving implementation of specific adapters to the community.
Unfortunately, as of now, these efforts have not really "took off".

## purpose

The goal of this module is to replicate historical data in the database of Zabbix to InfluxDB database(s).
Replication happens in real time,
therefore data is available in InfluxDB at the same time as in Zabbix DB.

Unfortunately, the information provided to modules by Zabbix server is very scarce: values themselves, timestamps and unique item identifiers.
To make sense of these data (specifically item identifiers),
one has to additionally use Zabbix API to find out item names, host names, etc.
Luckily, there is a piece of software which does exactly that - The Magnificent
[Zabbix plugin](https://grafana.com/plugins/alexanderzobnin-zabbix-app)
for
[Grafana](https://grafana.com)!

Since the large pile of historical data is mostly used in Zabbix for graphs,
offloading this task to InfluxDB and Grafana combo will make huge `history*` and `trends*` tables redundant.
You will need to keep the bare minimum there,
just enough to populate value cache upon Zabbix server restarts
and resolve `{ITEM.VALUE}` macros.
As we know, small tables are fast
and as a consequence database write operations will no longer limit the performance of Zabbix server!

## compilation

1. [Download](http://www.zabbix.com/download)
Zabbix source or check it out from
[SVN repository](https://www.zabbix.org/websvn/wsvn/zabbix.com?):
`svn checkout svn://svn.zabbix.com/branches/4.0`
> Version must be higher than 3.2 (when history export support was added). You need to compile module using sources of the version you will be using it with!

2. Place module source folder in Zabbix source tree `src/modules/` alongside `dummy`.

3. Install
[`libcurl`](https://curl.haxx.se/libcurl/)
and
[`libyaml`](https://pyyaml.org/wiki/LibYAML)
development packages.

4. Run `make` to build, it should produce `effluence.so` shared library.

## installation

1. Install
[`libcurl`](https://curl.haxx.se/libcurl/)
and
[`libyaml`](https://pyyaml.org/wiki/LibYAML)
in case you were compiling on a different machine.

2. Copy `effluence.so` to a desired location.

3. Set up necessary permissions.

## configuration

1. Create database(s) and user(s) in your InfluxDB instance(s).

2. Create module configuration file:
```yaml
url:  http://localhost:8086
db:   zabbix
user: effluence
pass: r3a11y_$tr0n9_pa$$w0rd
```

3. Set `EFFLU_CONFIG` environment variable for Zabbix server to the path to module configuration file.

4. Set `LoadModulePath` and `LoadModule` parameters in Zabbix
[server](https://www.zabbix.com/documentation/4.0/manual/appendix/config/zabbix_server)
configuration file.
```
LoadModulePath=/path/to/zabbix/modules
LoadModule=effluence.so
```

5. Restart Zabbix server.
