#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char *escape(const char *s) {
    int len = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') len += 2;
        else len += 1;
    }
    char *out = malloc(len + 1);
    if (!out) return NULL;
    int j = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') out[j++] = '\\';
        out[j++] = *p;
    }
    out[j] = 0;
    return out;
}

char *jobj(const char *type, ...) {
    va_list ap;
    char *buf = malloc(4096);
    if (!buf) return NULL;
    int off = snprintf(buf, 4096, "{\"type\":\"%s\"", type);

    va_start(ap, type);
    const char *key = va_arg(ap, const char *);
    while (key) {
        const char *val = va_arg(ap, const char *);
        if (!val) break;
        char *ekey = escape(key);
        char *eval = escape(val);
        off += snprintf(buf + off, 4096 - off, ",%s", "");
        off += snprintf(buf + off, 4096 - off, "\"%s\":\"%s\"", ekey, eval);
        free(ekey);
        free(eval);
        key = va_arg(ap, const char *);
    }
    va_end(ap);

    snprintf(buf + off, 4096 - off, "}");
    return buf;
}

char *jarr(const char *type, const char *arrkey, const char *arrjson) {
    char *buf = malloc(8192);
    if (!buf) return NULL;
    snprintf(buf, 8192, "{\"type\":\"%s\",\"%s\":%s}", type, arrkey, arrjson);
    return buf;
}

static char *extract_str(const char *json, const char *key) {
    char pat[256];
    snprintf(pat, 256, "\"%s\":", key);
    const char *start = strstr(json, pat);
    if (!start) return NULL;
    start += strlen(pat);
    while (*start == ' ') start++;
    if (*start != '"') return NULL;
    start++;
    char *out = malloc(4096);
    if (!out) return NULL;
    int i = 0;
    while (*start && *start != '"' && i < 4095) {
        if (*start == '\\' && *(start+1) == '"') {
            out[i++] = '"';
            start += 2;
        } else if (*start == '\\' && *(start+1) == '\\') {
            out[i++] = '\\';
            start += 2;
        } else {
            out[i++] = *start++;
        }
    }
    out[i] = 0;
    return out;
}

char *jstr(const char *json, const char *key) {
    return extract_str(json, key);
}

char *jstra(const char *json, const char *key, int idx) {
    char pat[256];
    snprintf(pat, 256, "\"%s\":", key);
    const char *p = json;
    int found = 0;
    while ((p = strstr(p, pat))) {
        if (found == idx) {
            p += strlen(pat);
            while (*p == ' ') p++;
            if (*p != '"') return NULL;
            p++;
            char *out = malloc(4096);
            if (!out) return NULL;
            int i = 0;
            while (*p && *p != '"' && i < 4095) {
                if (*p == '\\' && *(p+1) == '"') { out[i++] = '"'; p += 2; }
                else if (*p == '\\' && *(p+1) == '\\') { out[i++] = '\\'; p += 2; }
                else out[i++] = *p++;
            }
            out[i] = 0;
            return out;
        }
        found++;
        p++;
    }
    return NULL;
}

void jfree(char *json) {
    free(json);
}
