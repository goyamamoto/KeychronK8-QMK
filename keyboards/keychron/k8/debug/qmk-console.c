// Minimal QMK console reader (usage page 0xFF31) for macOS/Linux via hidapi.
// Usage: qmk-console [vid pid]   (default 3434 fe0e)
#include <hidapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    unsigned short vid = argc > 2 ? (unsigned short)strtoul(argv[1], NULL, 16) : 0x3434;
    unsigned short pid = argc > 2 ? (unsigned short)strtoul(argv[2], NULL, 16) : 0xfe0e;
    setvbuf(stdout, NULL, _IOLBF, 0);
    hid_init();
    for (;;) {
        hid_device *h = NULL;
        struct hid_device_info *devs = hid_enumerate(vid, pid);
        for (struct hid_device_info *d = devs; d; d = d->next) {
            if (d->usage_page == 0xFF31) {
                h = hid_open_path(d->path);
                if (!h) fprintf(stderr, "open failed: %ls\n", hid_error(NULL));
                break;
            }
        }
        hid_free_enumeration(devs);
        if (!h) {
            fprintf(stderr, "waiting for %04x:%04x console...\n", vid, pid);
            sleep(1);
            continue;
        }
        fprintf(stderr, "connected\n");
        unsigned char buf[65];
        for (;;) {
            int n = hid_read_timeout(h, buf, sizeof buf, 1000);
            if (n < 0) break;
            for (int i = 0; i < n && buf[i]; i++) putchar(buf[i]);
        }
        hid_close(h);
        fprintf(stderr, "disconnected\n");
    }
}
