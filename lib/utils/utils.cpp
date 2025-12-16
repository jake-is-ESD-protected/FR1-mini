#include <string.h>
#include "syserr.h"
#include "stdio.h"
#include <ctype.h>

char *strremove(char *str, const char *sub) {
    char *p, *q, *r;
    if (*sub && (q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL) {
            while (p < r)
                *q++ = *p++;
        }
        while ((*q++ = *p++) != '\0')
            continue;
    }
    return str;
}

e_syserr_t uint_to_4digit_str(uint16_t num, char* buf) {
    if(num >= 10000) return e_syserr_param;
    if(buf == NULL) return e_syserr_null;
    if(strlen(buf) < 4) return e_syserr_oom;
    snprintf(buf, 5, "%04d", num);
    return e_syserr_none;
}

e_syserr_t str_to_4digit_uint(const char* str, uint16_t* num) {
    if (str == NULL || num == NULL) return e_syserr_null;
    if (strlen(str) != 4) return e_syserr_param;
    for (int i = 0; i < 4; i++) {
        if (!isdigit(str[i])) return e_syserr_param;
    }
    *num = (uint16_t)strtoul(str, NULL, 10);
    return e_syserr_none;
}