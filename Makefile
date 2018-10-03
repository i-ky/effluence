effluence: src/*.c src/*.h
	cc -fPIC -shared -o effluence.so src/*.c -I$(ZABBIX_SOURCE)/include `curl-config --cflags --libs`
