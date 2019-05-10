# compile

1. [Download](http://www.zabbix.com/download)
Zabbix source or check it out from
[Git repository](https://git.zabbix.com/):
```bash
git clone https://git.zabbix.com/scm/zbx/zabbix.git --depth 1
```
> Version must be higher than 3.2
(when history export support was added).
You need to compile module using sources of the version you will be using it with!

2. Configure Zabbix sources:
```bash
cd /path/to/zabbix/source
./configure
```

3. Install
[`libcurl`](https://curl.haxx.se/libcurl/)
and
[`libyaml`](https://pyyaml.org/wiki/LibYAML)
development packages,
sort of:
 * CentOS:
```bash
sudo yum install libcurl-devel libyaml-devel
```
 * Ubuntu:
```bash
sudo apt install libcurl4-openssl-dev libyaml-dev
```

4. Get module sources,
point them to Zabbix source directory
and run `make` to build,
it should produce `effluence.so` shared library.
```bash
cd /path/to/effluence/source
export ZABBIX_SOURCE=/path/to/zabbix/source
make
```
