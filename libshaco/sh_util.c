#include "sh_util.h"
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

/*
static const char ENC_T[64] = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char DEC_T[256] = {
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, 62, -1, -1, -1, 63, 52, 53,
54, 55, 56, 57, 58, 59, 60, 61, -1, -1,
-1, -1, -1, -1, -1,  0,  1,  2,  3,  4,
 5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
49, 50, 51, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1,
};

int
sh_bytestr_encode(const uint8_t* bytes, int nbyte, char* str, int n) {
    uint8_t lastoff = 0;
    int i, j = 0;
    for (i=0; i<nbyte; ) {
        if (j >= n-1) {
            str[j] = '\0';
            return i;
        }
        if (lastoff > 0) {
            str[j] = bytes[i-1] >> lastoff;
            lastoff-=2;
            if (lastoff > 0) {
                str[j] |= (bytes[i++] << (6-lastoff)) & 0x3f;
            }
        } else {
            str[j] = bytes[i++] & 0x3f;
            lastoff = 6;
        }
        str[j] = ENC_T[(int)str[j]];
        j++;
    }
    if (lastoff > 0) {
        str[j] = bytes[i-1] >> lastoff;
        str[j] = ENC_T[(int)str[j]];
        j++;
    }
    str[j] = '\0';
    return nbyte;
}

int
sh_bytestr_decode(const char* str, int len, uint8_t* bytes, int nbyte) {
    uint8_t c;
    uint8_t lastoff = 0;
    int i, j = 0;
    for (i=0; i<len; ++i) {
        c = DEC_T[(unsigned char)(str[i])];
        if (c == -1) {
            return i;
        } 
        if (lastoff > 0) {
            bytes[j-1]|= c << lastoff;
            lastoff-=2;
            if (lastoff > 0) {
                if (j < nbyte) {
                    bytes[j++] = c >> (6-lastoff);
                } else {
                    return i+1;
                }
            }
        } else {
            if (j < nbyte) {
                bytes[j++] = c;
                lastoff = 6;
            } else {
                return i;
            }
        }
    }
    return len;
}
*/

int
sh_bytestr_encode(const uint8_t* bytes, int nbyte, char* str, int n) {
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
sh_bytestr_decode(const char* str, int len, uint8_t* bytes, int nbyte) {
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

int
sh_fork(char *const argv[], int n) {
    assert(n > 0);
    if (argv[n-1] != NULL) {
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid == 0) {
        pid_t pid2 = fork();
        if (pid2 < 0) {
            exit(0);
        }
        if (pid2 == 0) {
            execvp(argv[0], argv);
            // !!! do not call exit(1), 
            // exit will call the function register in atexit,
            // this will call net fini, del epoll_ctl event
            // eg: epoll_ctl del event, listen socket disabled!
            _exit(1);
            return 0;
        } else {
            // !!! do not call exit(1)
            _exit(1);
            return 0;
        }
    } else {
        if (waitpid(pid, NULL, 0) != pid)
            return 1;
        return 0;
    }
}
