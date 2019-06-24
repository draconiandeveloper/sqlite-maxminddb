set LIBMAXMINDDBPATH=libmaxminddb

cl /D_USRDLL /D_WINDLL /D UNICODE /I. /I%LIBMAXMINDDBPATH%\include /I%LIBMAXMINDDBPATH%\projects\VS12 sqlite3_maxminddb.c %LIBMAXMINDDBPATH%\src\maxminddb.c %LIBMAXMINDDBPATH%\src\data-pool.c Ws2_32.lib /link /DLL /OUT:maxminddb.dll
