CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS += $(shell $(PKG_CONFIG) --cflags sni-exporter gio-2.0 glib-2.0 cairo)
LDLIBS += $(shell $(PKG_CONFIG) --libs sni-exporter gio-2.0 glib-2.0 cairo)

TARGET = minimal-tray
SRC = minimal-tray.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
