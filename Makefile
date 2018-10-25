CFLAGS:=-fPIC -I$(ZABBIX_SOURCE)/include $(CFLAGS)
OBJECTS:=$(patsubst %.c,%.o,$(wildcard src/*.c))

effluence.so: $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) `curl-config --libs` -lyaml $(OBJECTS) -shared -o $@

all: effluence.so

clean:
	rm -rf $(OBJECTS) effluence.so
