#ifndef DCMUTIL_H
#define DCMUTIL_H

char *dcmFileAlloc(const char *filename, int *len);

void dcmCanonicalizePath(char **dspath, const char *curDir); // expecting a dynString

void daPushString(char ***da, const char *s);
void daPopString(char ***da);
void daPushUniqueString(char ***da, const char *s);
const char *daTopString(char ***da);

#endif
