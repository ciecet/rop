#ifndef BASE64_H
#define BASE64_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// returns decoded data length
int base64_decode (const char *text, uint8_t *dst, int textLen);
// returns encoded text length (trailing '\0' is not appended nor counted)
int base64_encode (const uint8_t *src, char *text, int dataLen);

#ifdef __cplusplus
}
#endif
#endif
