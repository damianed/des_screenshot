#ifndef INCLUDE_DES_TIME_DEBUG_H
#define INCLUDE_DES_TIME_DEBUG_H

#define MAX_DEBUGS 1024

long long unsigned getTimeMs();

void des_start_debug(char *id);

void des_end_debug(char *id);

void des_print_all_debugs();

void des_print_debug(char *id);

void des_clear_debugs();

#ifdef DES_TIME_DEBUG_IMPLEMENTATION

#define des_uint64 long long unsigned
#define des_uint unsigned int
#define des_b32 des_uint

typedef struct {
    des_uint64 start, end;
} des_TimeDebug;

des_TimeDebug debugs[MAX_DEBUGS];
char *debugs_ids[MAX_DEBUGS];
des_uint debug_count = 0;

long long unsigned des_getTimeMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    ts.tv_sec = ts.tv_sec < 0 ? 0 : ts.tv_sec;
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

des_b32 strEquals(char *s1, char *s2) {
#define MAX_LEN 255
    des_uint count = 0;
    while (count++ < MAX_LEN) {
        if ((*s1 == '\0' || *s2 == '\0') || (*s1++ != *s2++)) {
            break;
        }
    }

    if (*s1 == '\0' && *s2 == '\0') {
        return 1;
    }

    return 0;
}

des_TimeDebug *des_getDebug(char *id) {
    for (des_uint i = 0; i < debug_count; i++) {
        if (strEquals(debugs_ids[i], id)) {
            return &debugs[i];
        };
    }

    return 0;
}

void des_start_debug(char *id) {
    des_uint64 start = des_getTimeMs();
    des_TimeDebug *debug = des_getDebug(id);
    if (!debug) {
        debugs[debug_count] = (des_TimeDebug) {0, 0};
        debugs_ids[debug_count] = id;
        debug = &debugs[debug_count];

        debug_count++;
    }

    debug->start = start;
}

void des_end_debug(char *id) {
    des_uint64 end = des_getTimeMs(id);
    des_TimeDebug *debug = des_getDebug(id);

    if (debug) {
        debug->end = end;
    } else {
        printf("DES_DEBUG: id: %s not found\n", (char *) id);
    }
}

void des_print_all_debugs() {
    for (des_uint i = 0; i < debug_count; i++) {
        des_uint64 result = debugs[i].end - debugs[i].start;
        printf("DEBUG: TIME TAKEN BY %s:     %llums\n", debugs_ids[i], result);
    }
}

void des_print_debug(char *id) {
    des_TimeDebug *debug = des_getDebug(id);
    des_uint64 result = debug->end - debug->start;
    printf("DEBUG: TIME TAKEN BY %s:     %llums\n", id, result);
}

void des_clear_debugs() {
    debug_count = 0;
}

#endif
#endif
