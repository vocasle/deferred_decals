#include "myutils.h"

#include <stdarg.h>
#include <stdio.h>
//#include <debugapi.h>
//#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

void UtilsDebugPrint(const char* fmt, ...)
{
    char out[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);
 //   OutputDebugStringA(out);
    fprintf(stdout, "%s\n", out);
}

void UtilsFatalError(const char* fmt, ...)
{
    char out[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);
    fprintf(stderr, "%s\n", out);
//    OutputDebugStringA(out);
    exit(EXIT_FAILURE);
//    ExitProcess(EXIT_FAILURE);
}

const char* UtilsFormatStr(const char* fmt, ...)
{
    static char out[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);
    return out;
}

int UtilsStrFindLastChar(const char* str, const char ch)
{
    int pos = -1;
    const char* begin = str;
    while (str && *str)
    {
        if (*str == ch)
            pos = (int)(str - begin);
        ++str;
    }
    return pos;
}

const char *UtilsGetStrAfterChar(const char *str, const char ch)
{
  const uint32_t len = strlen(str);
  if (!len) {
    return NULL;
  }

  const int n = UtilsStrFindLastChar(str, ch);
  if (n < 0 || n + 1 >= len) {
    return NULL;
  }
  return str + n + 1;
}

void UtilsStrSub(const char* str, uint32_t start, uint32_t end, char out[], uint32_t maxSize)
{
    const uint32_t len = (uint32_t)strlen(str);
    assert(start < len&& end < len);
    uint32_t max = len < maxSize ? len : maxSize - 1;
    max = max < end ? max : end;

    for (uint32_t i = 0; i < max; ++i)
    {
        out[i] = str[i];
    }
    out[max] = 0;
}

unsigned char* UtilsReadData(const char* filepath, unsigned int* bufferSize)
{
    FILE* f = fopen(filepath, "rb");
    if (!f)
    {
        UTILS_FATAL_ERROR("Failed to read data from %s", filepath);
    }

    struct stat sb;
    if (stat(filepath, &sb) == -1)
    {
        UTILS_FATAL_ERROR("Failed to get file stats from %s", filepath);
    }
    unsigned char* bytes = malloc(sb.st_size);
    fread(bytes, sb.st_size, 1, f);
    fclose(f);
    *bufferSize = sb.st_size;
    return bytes;
}
