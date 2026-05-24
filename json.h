#ifndef JSON_H
#define JSON_H

char *jobj(const char *type, ...);
char *jarr(const char *type, const char *arrkey, const char *arrjson);
char *jstr(const char *json, const char *key);
char *jstra(const char *json, const char *key, int idx);
void jfree(char *json);

#endif
