#ifndef __elog_h__
#define __elog_h__

struct elog;
struct elog_appender;

struct elog* elog_create(const char* filename);
void elog_free(struct elog* self);
void elog_append(struct elog* self, const char* msg, int sz);
int  elog_set_appender(struct elog* self, const struct elog_appender* appender, const char *mode);

#endif
