#include <stdio.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>

extern int avpriv_tak_parse_streaminfo(void *s, const uint8_t *buf, int size);

int main() {
    uint8_t payload[] = {0x02, 0x0c, 0x11, 0x2b, 0x00, 0x00, 0x40, 0x4d, 0x09, 0x0a};
    // TAKStreamInfo is larger than 256 bytes, allocate enough
    uint8_t ti[1024] = {0};
    int ret = avpriv_tak_parse_streaminfo(ti, payload, 10);
    printf("avpriv_tak_parse_streaminfo returned: %d\n", ret);
    return 0;
}
