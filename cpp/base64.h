#ifndef BASE64_H
#define BASE64_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

int base64_decode(char *text, uint8_t *dst, int numBytes);
int base64_encode(uint8_t *src, char *text, int numBytes);

#ifdef __cplusplus
}
#endif
#endif
