CFLAGS:=-fPIC -I$(ZABBIX_SOURCE)/include -I$(ZABBIX_SOURCE)/include/common $(CFLAGS)
OBJECTS:=$(patsubst %.c,%.o,$(wildcard src/*.c))

effluence.so: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) `curl-config --libs` -lyaml -shared -o $@

all: effluence.so

clean:
	rm -rf $(OBJECTS) effluence.so
