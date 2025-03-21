#!/bin/sh
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libmaxminddb.so sqlite3 <<EOT
.load ./maxminddb
select geoip_asn_owner('8.8.8.8'), geoip_asn_number('8.8.8.8');
select geoip_continent('8.8.8.8'), geoip_country('8.8.8.8'), geoip_state('8.8.8.8'), geoip_city('8.8.8.8');
select geoip_zipcode('8.8.8.8'), geoip_timezone('8.8.8.8');
select geoip('8.8.8.8');
EOT
