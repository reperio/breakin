#include <stdio.h>


char *trim(char *string) {
    int start = 0;
    char *p;

    p = string;
    while ((*p == ' ') || (*p == '\t')) {
        start ++;
        p ++;
    }

    if (start) {
        while (*p) {
            *(p - start) = *p;
            p ++;
        }
        *(p - start) = '\0';
    }
    else {
        while (*p) {
            p ++;
        }
    }

    p -= start + 1;
    while ((*p == ' ') || (*p == '\n') || (*p == '\r') || (*p == '\t')) {
        *p = '\0';
        p --;
    }
    return string;
}
