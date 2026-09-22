#include <stdio.h>
#include "des_string_view.h"

#define MAX_ITERATION 1024

StringView strToStringView(char *s) {
    unsigned int count = 0;

    while (count < MAX_ITERATION) {
        if (s[count] == '\0') {
            break;
        }

        count++;
    }

    if (count >= MAX_ITERATION) {
        fprintf(stderr, "Max iteration reached in strToStringView\n");
    }

    return (StringView) {s, count};
}

bool strEquals(char *s1, char *s2) {
    unsigned int count = 0;
    while (count++ < MAX_ITERATION) {
        if ((*s1 == '\0' || *s2 == '\0') || (*s1++ != *s2++)) {
            break;
        }
    }

    if (*s1 == '\0' && *s2 == '\0') {
        return 1;
    }

    return 0;
}

bool strViewEquals(StringView *s1, StringView *s2) {
    if (s1->size != s2->size)  {
        return 0;
    }

    unsigned int index = 0;
    while (index < MAX_ITERATION) {
        if (s1->size <= index) {
            return 1;
        }

        if (s1->data[index] != s2->data[index]) {
            break;
        }

        index++;
    }

    if (index >= MAX_ITERATION) {
        fprintf(stderr, "Max iteration reached in strEquals\n");
    }

    return 0;
}

bool strViewStartsWith(StringView *s, StringView *subs) {
    if (s->size < subs->size) {
        return 0;
    }

    for (unsigned int i = 0; i < subs->size; i++) {
        if (s->data[i] != subs->data[i]) {
            return 0;
        }
    }

    return 1;
}

StringView strViewSplit(StringView *s, char separator)  {
    unsigned int count = 0;
    while (count < s->size && s->data[count] != separator) {
        count++;
    }

    char *newData = s->data;
    strViewTrimCharsLeft(s, count + 1);

    return (StringView) {newData, count};
}

void strViewTrimCharsLeft(StringView *s, int n) {
    if (n > (signed int)s->size) {
        n = s->size;
    }

    s->data += n;
    s->size -= n;
}

void strViewTrimCharsRight(StringView *s, int n) {
    if (n > (signed int)s->size) {
        n = s->size;
    }

    s->size -= n;
}
