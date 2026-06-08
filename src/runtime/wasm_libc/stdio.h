#ifndef SPRY_FREESTANDING_STDIO_H
#define SPRY_FREESTANDING_STDIO_H
#include <stddef.h>
typedef struct _spry_FILE FILE;
FILE* fopen(const char*, const char*);
int fclose(FILE*);
size_t fread(void*, size_t, size_t, FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
#endif
