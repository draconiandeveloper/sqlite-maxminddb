# sqlite-maxminddb

My own modified fork of [ng-labo's code](https://github.com/ng-labo/sqlite-maxminddb)

## Brief

This SQLite3 extension provides the following functions:
```
geoip_asn_number(ipaddr) : Retrieve the Autonomous System Number

geoip_asn_owner(ipaddr)  : Retrieve the Autonomous System Number and the organization that owns it

geoip_continent(ipaddr)  : Retrieve the continent that is associated with the address

geoip_country(ipaddr)    : Retrieve the country that is associated with the address

geoip_state(ipaddr)      : Retrieve the state if the country is the United States

geoip_city(ipaddr)       : Retrieve the city if the country is the United States

geoip_zipcode(ipaddr)    : Retrieve the Zone Improvement Plan Code if the country is the United States

geoip_timezone(ipaddr)   : Retrieve the IANA time zone that is associated with the address

geoip(ipaddr)            : Retrieves all of the above separated by " | "
```

## Compiling and Testing

1. Pull the source code from this repository
    - `git pull https://github.com/draconiandeveloper/sqlite-maxminddb`

2. Enter the new directory
    - `cd sqlite-maxminddb`

3. Build this program
    - `mkdir build && cd build`
    - `cmake -DCMAKE_BUILD_TYPE=Release ..`
    - `make`

4. Copy the `GeoLite2-ASN.mmdb` and `GeoLite2-City.mmdb` files to the same directory as:
    - **Linux:** `libmaxminddb_ext.so`
    - **Windows:** `maxminddb_ext.dll`
    - **macOS:** `libmaxminddb_ext.dylib`

5. Run SQLite3 as you normally would.
    - Since `libmaxminddb` was statically compiled and linked, we no longer have to use the `LD_PRELOAD` method.

6. Execute the following query in SQLite3
    - **Linux:** `.load ./libmaxminddb_ext`
    - **Windows:** `.load ./maxminddb_ext`
    - **macOS:** `.load ./libmaxminddb_ext`

From there you can start pulling data from the MaxMind database files.

## Notes

- This extension has only been tested on the GeoIP2-Lite *(GeoLite2)* MaxMind database files.
    * There's no guarantee that this extension will work for other GeoIP database formats.
- I have not personally tested this code out on a Windows or macOS machine.
    * Plus your mileage might vary on Linux if you're using MUSL, uClibc, or any other non-Glibc C library.

## TODOs

- [x] Include a CMake build method for this extension that automates the process of pulling and compiling `libmaxminddb`, linking dependencies, and running tests.
- [x] Remove the hard-coded paths for the GeoIP2-Lite MMDB files, maybe I'll have them load from the same directory where the extension shared library/DLL file is located for portability.
- [x] Implement C++Check tests to catch any potential bad coding habits.
- [ ] Overhaul the codebase further to make it more readable,
- [x] Implement Doxygen documentation
- [ ] Slim down the `libmaxminddb` codebase to make the code style more uniform.
- [ ] Add more SQLite3 error responses for unresolvable IP addresses and other potential issues.
- [ ] Run more tests to catch any issues that may be present such as IP addresses that might have more than one U.S. state associated with them *(which may crash SQLite3)*