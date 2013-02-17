#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void genHeader(FILE *inputFile, FILE *outputFile, const char *baseName)
{
    unsigned char *bytes;
    int i, size;

    fseek(inputFile, 0, SEEK_END);
    size = (int)ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    fprintf(outputFile, "unsigned int  %sSize   = %d;\n"
                        "unsigned char %sData[] = {\n",
            baseName,
            size,
            baseName);

    bytes = malloc(size);
    fread(bytes, 1, size, inputFile);
    for(i=0; i<size; i++)
    {
        fprintf(outputFile, "0x%2.2x", (int)bytes[i]);
        if(i < size-1)
            fprintf(outputFile, ",");
        if((i % 15) == 14)
            fprintf(outputFile, "\n");
    }
    fprintf(outputFile, "\n};\n");
    free(bytes);
}

int main(int argc, char* argv[])
{
    int i;
    const char *inputFilename  = NULL;
    const char *outputFilename = NULL;
    const char *baseName       = NULL;
    FILE *inputFile;
    FILE *outputFile;

    for(i=0; i<argc; i++)
    {
        if(!strcmp(argv[i], "-i") && (i+1 < argc))
        {
            inputFilename = argv[++i];
        }
        else if(!strcmp(argv[i], "-o") && (i+1 < argc))
        {
            outputFilename = argv[++i];
        }
        else if(!strcmp(argv[i], "-n") && (i+1 < argc))
        {
            baseName = argv[++i];
        }
    }

    if(!inputFilename
    || !outputFilename
    || !baseName)
    {
        printf("Syntax: genHeader -i [input binary filename] -o [output header filename] -n [namespace identifier to use]\n");
        return -1;
    }

    inputFile  = fopen(inputFilename,  "rb");
    if(!inputFile)
    {
        printf("genHeader ERROR: Can't open '%s' for read.\n", inputFilename);
        return -1;
    }

    outputFile = fopen(outputFilename, "wb");
    if(!outputFile)
    {
        fclose(inputFile);
        printf("genHeader ERROR: Can't open '%s' for write.\n", outputFilename);
        return -1;
    }

    genHeader(inputFile, outputFile, baseName);
    fclose(inputFile);
    fclose(outputFile);
    return 0;
}
