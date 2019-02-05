#!/bin/sh
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libmaxminddb.so sqlite3 <<EOT
.load ./maxminddb
select asn('192.168.0.1'), org('192.168.0.1'), cc('192.168.0.1');
select asn('1.1.1.1'), org('1.1.1.1'), cc('1.1.1.1');
select asn('8.8.8.8'), org('8.8.8.8'), cc('8.8.8.8');
select asn('2001:4860:4860::8888'), org('2001:4860:4860::8888'), cc('2001:4860:4860::8888');
select asn('210.188.224.11'), org('210.188.224.11'), cc('210.188.224.11');
select asn('192.168.0.1'), org('192.168.0.1'), cc('192.168.0.1');
EOT
