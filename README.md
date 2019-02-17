# effluence [![Travis CI Build Status](https://www.travis-ci.com/i-ky/effluence.svg?branch=master)](https://www.travis-ci.com/i-ky/effluence) [![Coverity Scan Build Status](https://scan.coverity.com/projects/17710/badge.svg)](https://scan.coverity.com/projects/i-ky-effluence)

[Zabbix](http://www.zabbix.com)
[loadable module](https://www.zabbix.com/documentation/current/manual/config/items/loadablemodules)
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

But everyone seems to agree that the ultimate solution would be to offload historical data into a more suitable storage engine.

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

## quick start

### get binaries

Check out
[the latest release](https://github.com/i-ky/effluence/releases/latest)
for precompiled binaries or
[compile](./COMPILE.md)
from sources yourself.

### install

1. Install
[`libcurl`](https://curl.haxx.se/libcurl/)
and
[`libyaml`](https://pyyaml.org/wiki/LibYAML)
in case you are using precompiled binaries or
were compiling on a different machine,
sort of:
 * CentOS:
```bash
sudo yum install libcurl libyaml
```
 * Ubuntu:
```bash
sudo apt install libcurl4 libyaml-0-2
```

2. Copy `effluence.so` to a desired location.
```bash
cp effluence.so /path/to/zabbix/modules/
```

3. Set up necessary permissions.

### configure

1. Create
[database(s)](https://docs.influxdata.com/influxdb/latest/introduction/getting-started/#creating-a-database)
and
[user(s)](http://docs.influxdata.com/influxdb/latest/administration/authentication_and_authorization)
in your InfluxDB instance(s).

2. Create module [configuration file](#configuration-file-format):
```yaml
url:  http://localhost:8086
db:   zabbix
user: effluence
pass: r3a11y_$tr0n9_pa$$w0rd
```
> This is a very simple configuration to export all data types.
However, if your only goal is integration with Grafana,
consider exporting only numeric data types (learn
[here](#configuration-file-for-exporting-numeric-values-only)
how to achieve this).
At the moment Zabbix plugin queries other data types from Zabbix API.

3. Set `EFFLU_CONFIG` environment variable for Zabbix server to the path to module configuration file.

4. Set `LoadModulePath` and `LoadModule` parameters in Zabbix
[server](https://www.zabbix.com/documentation/current/manual/appendix/config/zabbix_server)
configuration file.
```
LoadModulePath=/path/to/zabbix/modules
LoadModule=effluence.so
```

5. Restart Zabbix server.

### enjoy

But keep in mind that there is
[more](#boring-details)
to learn when you come back later.
You may want to fine-tune
[configuration](#configuration-file-format)
and know how to keep
[database size](#database-sizing)
under control.
If you are **not** planning to use Zabbix plugin for Grafana,
there is
[some useful information](#database-schema)
regarding data representation in InfluxDB.
It will be handy if you decide to
[explore your data](https://docs.influxdata.com/influxdb/latest/query_language/data_exploration)
using InfluxQL.

## boring details

### configuration file format

Module uses
[YAML Ain't Markup Language](http://yaml.org)
format of configuration file.
Here is the list of attributes one can specify there:

attribute |           | description
----------|-----------|------------
`url`     | mandatory | URL to send requests to (`/write` is added automatically)
`db`      | mandatory | database where to store data
`user`    | optional  | username to use for authentication
`pass`    | optional  | password to use for authentication

These attributes can be specified in the root of the document,
this way they will have global effect.
When different types need to be stored
in different InfluxDB instances,
different databases
or on behalf of different users,
same attributes can be specified per data type
(names should be familiar to anyone who has ever tried to
[setup Zabbix with Elasticsearch](https://www.zabbix.com/documentation/current/manual/appendix/install/elastic_search_setup)):

data type | *Type of information*
----------|----------------------
`dbl`     | *Numeric (float)*
`uint`    | *Numeric (unsigned)*
`str`     | *Character*
`text`    | *Text*
`log`     | *Log*

If there is no section for a specific data type,
module will use global attributes (if provided).
If the configuration of a particular data type lacks any one of mandatory attributes,
then the callback for that data type is not provided to Zabbix server
and the data of this type are not being exported.

#### configuration file examples

##### minimalist configuration file

When you have just installed InfluxDB
and have not enabled authentication yet,
you can use the following configuration.
All data types will be exported
and stored together.

```yaml
# one set of attributes for all data types
url:  http://localhost:8086
db:   zabbix
```

##### configuration with a special place for numeric types

With the following configuration all data types will be exported,
but `dbl` will be sent to a different URL,
while `uint` is stored to a different database.

```yaml
# global attributes (will be used for Character, Text and Log)
url:  http://localhost:8086
db:   zabbix
user: effluence
pass: r3a11y_$tr0n9_pa$$w0rd

dbl: # specifically for Numeric (float)
  url:  http://very.special.place
  db:   float_only
  user: ecneulffe
  pass: w34k_p4$$w0rd

uint: # specifically for Numeric (unsigned)
  url: http://only.for.unsigned
  db:  not_that_special_but_still
  # no authentication, global configuration does not apply
```

##### configuration file using YAML alias

This configuration file shows
how to avoid duplication
and copy-paste errors
using YAML *aliases* and *references*.

```yaml
str: # Character
  url: &url http://localhost:8086
  db: shorties

text: # Text
  url: *url # reference to &url
  db: longies
```

##### configuration file for exporting numeric values only

Another way to provide identical attributes for different data types is to use *array* of data types as *key* for type-specific attribute section.

```yaml
[dbl, uint]: # for both Numeric (float) and Numeric (unsigned)
  url: http://localhost:8086
  db: numeric
  user: effluence
  pass: r3a11y_$tr0n9_pa$$w0rd
```

### database schema

Regardless of configured `url` and `database`
each data type will be stored in separate
[measurement](https://docs.influxdata.com/influxdb/latest/concepts/glossary/#measurement).

*Type of information* | measurement
----------------------|------------
*Numeric (float)*     | `history`
*Numeric (unsigned)*  | `history_uint`
*Character*           | `history_str`
*Text*                | `history_text`
*Log*                 | `history_log`

As you see, measurement names are the same as table names in Zabbix DB.
There is a good reason for this.
Measurements are analogous to SQL tables and
having same names simplifies writing
[queries](https://docs.influxdata.com/influxdb/latest/query_language/data_exploration/)
for different backends.

All measurements will have the only [tag](https://docs.influxdata.com/influxdb/latest/concepts/glossary/#tag) named `itemid` and a [field](https://docs.influxdata.com/influxdb/latest/concepts/glossary/#field) named `value`.
Measurement `history_log` will additionally have `source`, `timestamp`, `logeventid` and `severity` fields.
And of course every datapoint in InfluxDB has a `time` associated with it.

One important thing to note is that *Numeric (unsigned)* values are stored as floats,
because unlike Zabbix which uses *unsigned* 64 bit integers,
InfluxDB prefers *signed* 64 bit integers.
Since it wouldn't be possible to squeeze largest Zabbix integers into InfluxDB integer type
and InfluxDB
[does not support fields changing type on the fly](https://docs.influxdata.com/influxdb/latest/write_protocols/line_protocol_reference/#field-type-discrepancies),
decision was made to store all numeric values as floats.
Yes, for largest values some precision will be lost,
but hopefully it won't be noticeable.

### database sizing

Fortunately, InfluxDB provides enough
[tools](https://docs.influxdata.com/influxdb/latest/guides/downsampling_and_retention/)
to solve this problem.
This allows to keep module simple and ~~stupid~~ reliable.
This also provides more flexibility to the end user because no parameters are hard coded.

InfluxDB has a mechanism of
[retention policies](https://docs.influxdata.com/influxdb/latest/concepts/glossary/#retention-policy-rp)
to keep the size of the database under control.
Please [create](https://docs.influxdata.com/influxdb/latest/query_language/database_management/#retention-policy-management)
one and make it `default`,
because module will only write data into default retention policy of the database.
Retention policies specify how long InfluxDB stores the data.
Without retention policy data is never deleted from InfluxDB and your database may grow out of control.

One more sensible thing to do is to aggregate older numeric data.
This will be good for both database size
and performance of queries with longer intervals.
In Zabbix this concept is known as
[history and trends](https://www.zabbix.com/documentation/current/manual/config/items/history_and_trends),
in InfluxDB same can be achieved using
[continuous queries](https://docs.influxdata.com/influxdb/latest/query_language/continuous_queries).
[Create](https://docs.influxdata.com/influxdb/latest/query_language/database_management/#retention-policy-management)
one more retention policy for trends (non-`default` this time) with longer (or even infinite) storage duration.
Populate it with a couple of continuous queries like these two
(assuming database is called `zabbix`
and retention policy is `longterm`):
```sql
CREATE CONTINUOUS QUERY trends ON zabbix
BEGIN
    SELECT
        count(value) AS num,
        min(value) AS value_min,
        mean(value) AS value_avg,
        max(value) AS value_max
    INTO
        zabbix.longterm.:MEASUREMENT
    FROM
        /history(?:_uint)?$/
    GROUP BY
        time(1h,2h),*
END
```
> Users of Zabbix plugin for Grafana should refer to plugin's documentation to get the **exact** measurement and field names.
Otherwise plugin won't be able to find the data!

InfluxDB lets user to be creative and does not limit you to one aggregation stage.
For example,
you can keep raw data for a day
and aggregate it on an hourly basis.
Then you can keep hourly data for a week
and further aggregate it on a daily basis
before storing it forever.
