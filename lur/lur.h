#ifndef __lur_h__
#define __lur_h__

#define LUR_OK(r) (r[0] == '\0')

struct lur;
int lur_getint(struct lur* self, const char* key, int def);
float lur_getfloat(struct lur* self, const char* key, float def);
const char* lur_getstr(struct lur* self, const char* key, const char* def);

int lur_getnode(struct lur* self, const char* key);
int lur_nextnode(struct lur* self);

const char* lur_dofile(struct lur* self, const char* file);
struct lur* lur_create();
void lur_free(struct lur* self);

#endif
