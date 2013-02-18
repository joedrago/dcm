#include "dcmUtil.h"

#include "dyn.h"

#include <stdlib.h>
#include <string.h>
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

static int isAbsolutePath(const char *s)
{
    if(s[0] == '/')
    {
        // dumb, unix specific way for now
        return 1;
    }
    return 0;
}

#ifdef WIN32
#define PROPER_SLASH '\\'
#define PROPER_CURRENT "\\."
#define PROPER_PARENT "\\.."
#else
#define PROPER_SLASH '/'
#define PROPER_CURRENT "/."
#define PROPER_PARENT "/.."
#endif

static void dsCleanupSlashes(char **ds)
{
    char *c;
    char *head;
    char *tail;
    int wasSlash = 0;
    if(!dsLength(ds))
    {
        return;
    }

    // Remove all trailing slashes
    c = *ds + (dsLength(ds) - 1);
    for(; c >= *ds; --c)
    {
        if((*c == '/') || (*c == '\\'))
        {
            *c = 0;
        }
        else
        {
            break;
        }
    }

    // Remove all duplicate slashes, fix slash direction
    head = *ds;
    tail = *ds;
    for(; *tail; ++tail)
    {
        int isSlash = ((*tail == '/') || (*tail == '\\')) ? 1 : 0;
        if(isSlash)
        {
            if(!wasSlash)
            {
                *head = PROPER_SLASH;
                ++head;
            }
        }
        else
        {
            *head = *tail;
            ++head;
        }
        wasSlash = isSlash;
    }
    *head = 0;

    // Clue in the dynString that it was manually altered
    dsCalcLength(ds);
}

static void dsSquashDotDirs(char **ds)
{
    char *dot;
    char *prevSlash;
    while((dot = strstr(*ds, PROPER_PARENT)) != NULL)
    {
        if(dot == *ds)
        {
            // bad path
            return;
        }
        prevSlash = dot - 1;
        dot += strlen(PROPER_PARENT);
        while((prevSlash != *ds) && (*prevSlash != PROPER_SLASH))
        {
            --prevSlash;
        }
        if(prevSlash != *ds)
        {
            memmove(prevSlash, dot, strlen(dot)+1);
        }
    }
    while((dot = strstr(*ds, PROPER_CURRENT)) != NULL)
    {
        if(dot == *ds)
        {
            // bad path
            return;
        }
        prevSlash = dot;
        dot += strlen(PROPER_CURRENT);
        if(prevSlash != *ds)
        {
            memmove(prevSlash, dot, strlen(dot)+1);
        }
    }
    dsCalcLength(ds);
}

void dcmCanonicalizePath(char **dspath, const char *curDir)
{
    char *temp = NULL;
    if(!curDir || !strlen(curDir))
    {
        curDir = ".";
    }

    if(isAbsolutePath(*dspath))
    {
        dsCopy(&temp, *dspath);
    }
    else
    {
        dsPrintf(&temp, "%s/%s", curDir, *dspath);
    }

    dsCleanupSlashes(&temp);
    dsSquashDotDirs(&temp);

    dsCopy(dspath, temp);
    dsDestroy(&temp);
}

void daPushString(char ***da, const char *s)
{
    int index = daPush0(da);
    char **ds = &(*da)[index];
    dsCopy(ds, s);
}

void daPopString(char ***da)
{
    if(daSize(da) > 0)
    {
        daSetSize(da, daSize(da) - 1, dsDestroyIndirect);
    }
}

void daPushUniqueString(char ***da, const char *s)
{
    int i;
    for(i = 0; i < daSize(da); ++i)
    {
        if(!strcmp((*da)[i], s))
        {
            return;
        }
    }
    daPushString(da, s);
}

const char *daTopString(char ***da)
{
    if(daSize(da) > 0)
    {
        return (*da)[daSize(da) - 1];
    }
    return NULL;
}

