TARGET = maxminddb.so

$(TARGET): sqlite3_maxminddb.c
	gcc -Wall -shared -fPIC sqlite3_maxminddb.c -o maxminddb.so

clean:
	rm -rf $(TARGET)

test:
	simple_test.sh

