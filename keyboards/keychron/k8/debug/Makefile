# QMK console reader for the ITON_BT_DEBUG build (needs hidapi and pkg-config;
# macOS: brew install hidapi pkgconf).
qmk-console: qmk-console.c
	$(CC) -O2 -Wall -o $@ $< $$(pkg-config --cflags --libs hidapi)

clean:
	rm -f qmk-console

.PHONY: clean
