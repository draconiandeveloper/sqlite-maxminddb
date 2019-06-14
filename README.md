# sqlite-maxminddb

This SqLite extension provides `asn()` `org()` `cc()` functions for lookup ASN, CountryCode from IP adresses.
`ipmask(), ip6mask()' help to translate to prefixes easily.

## Usage

- prepares libmaxminddb
install from package system, or build from source.

- downloads geolite2 (its one of available contents)
put $HOME/.maxminddb/

```
$ make
$ LD_PRELOAD=libmaxminddb.so sqlite3
sqlite> .load ./maxminddb
sqlite> select asn('8.8.8.8'), org('8.8.8.8'), cc('8.8.8.8');
15169|Google LLC|US
sqlite> select asn('192.168.1.1'), org('192.168.1.1'), cc('192.168.1.1');
||
sqlite> select ipmask('192.168.1.1', 24);
192.168.1.0
```

## NOTE
- This extension depends on GeoIP2-Lite, or it is other way to create a custom MaxMind DB File.
- It will process no error in the case of success IP lookup and failure getting value. Unresolveble IP is usual.
- This extension loads mmdb file from fixed directory. I'd like to consider better way to load any mmdb file to library.
- It is useful to write '.load /pathto/maxminddb' into $HOME/.sqliterc.
- Although maxmind/libmaxminddb is available on Windows/OS X, I don't cater for them. Now I only check on x86_64 on ubuntu 16.

