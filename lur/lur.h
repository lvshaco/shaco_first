#ifndef __lur_h__
#define __lur_h__

#define LUR_OK(R) (R[0] == '\0')

struct lur;
int lur_getint(struct lur* self, const char* key, int def);
const char* lur_getstr(struct lur* self, const char* key, const char* def);

const char* lur_dofile(struct lur* self, const char* file);
struct lur* lur_create();
void lur_free(struct lur* self);

#endif
