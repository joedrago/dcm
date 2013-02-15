#include "dcmUtil.h"

#include <stdlib.h>
#include <stdio.h>

char *dcmFileAlloc(const char *filename, int *outputLen)
{
    char *data;
    int len;
    int bytesRead;

    FILE *f = fopen(filename, "rb");
    if(!f)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    len = (int)ftell(f);
    if(len < 1)
    {
        fclose(f);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);
    data = (char *)calloc(1, len + 1);
    bytesRead = fread(data, 1, len, f);
    if(bytesRead != len)
    {
        free(data);
        data = NULL;
    }

    fclose(f);
    *outputLen = len;
    return data;
}
