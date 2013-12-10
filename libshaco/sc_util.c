#include "sc_util.h"

int
sc_bytestr_encode(const uint8_t* bytes, int nbyte, char* str, int n) {
    uint8_t lastoff = 0;
    int i, j = 0;
    for (i=0; i<nbyte; ) {
        if (j >= n-1) {
            str[j] = '\0';
            return i;
        }
        if (lastoff > 0) {
            str[j] = bytes[i-1] >> lastoff;
            lastoff--;
            if (lastoff > 0) {
                str[j] |= (bytes[i++] << (7-lastoff)) & 0x7f;
                str[j] += 1;
            } else {
                str[j] += 1;
            }           
        } else {
            str[j] = bytes[i++] & 0x7f;
            str[j] += 1;
            lastoff = 7;
        }
        j++;
    }
    if (lastoff > 0) {
        str[j] = bytes[i-1] >> lastoff;
        str[j++] += 1;
    }
    str[j] = '\0';
    return nbyte;
}

int
sc_bytestr_decode(const char* str, int len, uint8_t* bytes, int nbyte) {
    uint8_t c;
    uint8_t lastoff = 0;
    int i, j = 0;
    for (i=0; i<len; ++i) {
        c = (str[i]-1) & 0x7f;
        if (lastoff > 0) {
            bytes[j-1]|= c << lastoff;
            lastoff--;
            if (lastoff > 0) {
                if (j < nbyte) {
                    bytes[j++] = c >> (7-lastoff);
                } else {
                    return i;
                }
            }
        } else {
            if (j < nbyte) {
                bytes[j++] = c;
                lastoff = 7;
            } else {
                return i;
            }
        }
    }
    return len;
}
