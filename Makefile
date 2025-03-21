TARGET = maxminddb.so

$(TARGET): sqlite3_maxminddb.c
	@gcc -Wall -shared -fPIC sqlite3_maxminddb.c -lmaxminddb -lsqlite3 -o $(TARGET)

clean:
	@rm -f $(TARGET)