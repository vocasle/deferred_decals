#include "myutils.h"

#include <stdarg.h>
#include <stdio.h>
// #include <debugapi.h>
// #include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#if _WIN32
#include <Windows.h>
#endif

void
UtilsDebugPrint(const char *fmt, ...)
{
    char out[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);
    //   OutputDebugStringA(out);
    fprintf(stdout, "%s\n", out);
}

void
UtilsFatalError(const char *fmt, ...)
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

const char *
UtilsFormatStr(const char *fmt, ...)
{
    static char out[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);
    return out;
}

int
UtilsStrFindLastChar(const char *str, const char ch)
{
    int pos = -1;
    const char *begin = str;
    while (str && *str) {
        if (*str == ch)
            pos = (int)(str - begin);
        ++str;
    }
    return pos;
}

const char *
UtilsGetStrAfterChar(const char *str, const char ch)
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

void
UtilsStrSub(const char *str, uint32_t start, uint32_t end, char out[],
            uint32_t maxSize)
{
    const uint32_t len = (uint32_t)strlen(str);
    assert(start < len && end < len);
    uint32_t max = len < maxSize ? len : maxSize - 1;
    max = max < end ? max : end;

    for (uint32_t i = 0; i < max; ++i) {
        out[i] = str[i];
    }
    out[max] = 0;
}

unsigned char *
UtilsReadData(const char *filepath, unsigned int *bufferSize)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        UTILS_FATAL_ERROR("Failed to read data from %s", filepath);
    }

    struct stat sb;
    if (stat(filepath, &sb) == -1) {
        UTILS_FATAL_ERROR("Failed to get file stats from %s", filepath);
    }
    unsigned char *bytes = malloc(sb.st_size);
    fread(bytes, sb.st_size, 1, f);
    fclose(f);
    *bufferSize = sb.st_size;
    return bytes;
}

#define DIRECTORY_NAME_MAX_LENGTH 255
#define DIRECTORY_STACK_MIN_CAPACITY 8
struct DirectoryStack {
    char **directories;
    uint32_t numDirectories;
    uint32_t capacity;
};

struct DirectoryStack *
DirectoryStack_Create(void)
{
    struct DirectoryStack *stack = malloc(sizeof *stack);
    ZERO_MEMORY_SZ(stack, sizeof(struct DirectoryStack));
    stack->directories = malloc(sizeof(char *) * DIRECTORY_STACK_MIN_CAPACITY);
    ZERO_MEMORY_SZ(stack->directories,
                   sizeof(char *) * DIRECTORY_STACK_MIN_CAPACITY);
    stack->capacity = DIRECTORY_STACK_MIN_CAPACITY;
    return stack;
}

void
DirectoryStack_Destroy(struct DirectoryStack *stack)
{
    for (uint32_t i = 0; i < stack->numDirectories; ++i) {
        free(stack->directories[i]);
    }
    free(stack->directories);
    free(stack);
    stack = NULL;
}

const char *
DirectoryStack_Pop(struct DirectoryStack *stack)
{

    const char *directory = stack->directories[stack->numDirectories - 1];
    stack->numDirectories--;
    return directory;
}

void
DirectoryStack_Push(struct DirectoryStack *stack, const char *directory)
{
    if (stack->numDirectories + 1 == stack->capacity) {
        stack->capacity = stack->capacity * 2;
        stack->directories
            = realloc(stack->directories, sizeof(char *) * stack->capacity);
    }

    const uint32_t len = strlen(directory) + 1;
    stack->directories[stack->numDirectories] = malloc(len);
    ZERO_MEMORY_SZ(stack->directories[stack->numDirectories], len);
    memcpy(stack->directories[stack->numDirectories], directory, len);
    stack->numDirectories++;
}

void
UtilsFileArray_Destroy(struct UtilsFileArray *array)
{
    free(array->files);
    free(array);
    array = NULL;
}

struct UtilsFileArray *
UtilsFileUtilsWalkDirectory(const char *directory)
{

#if _WIN32
    const char *root = NULL;
    struct DirectoryStack *stack = DirectoryStack_Create();
    DirectoryStack_Push(stack, directory);
    WIN32_FIND_DATA findData = { 0 };
    const char *excludes[] = { ".", ".." };
    struct UtilsFileArray *arr = malloc(sizeof *arr);
    ZERO_MEMORY_SZ(arr, sizeof(struct UtilsFileArray));
    LARGE_INTEGER fileSize = { 0 };

    while (stack->numDirectories > 0) {
        root = DirectoryStack_Pop(stack);
        HANDLE file = FindFirstFile(UtilsFormatStr("%s/*", root), &findData);
        if (!file) {
            UtilsDebugPrint("ERROR: Failed to find %s", directory);
            free(arr);
            arr = NULL;
            break;
        }

        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                UtilsDebugPrint("Directory: %s/%s", root, findData.cFileName);
                if (strcmp(findData.cFileName, excludes[0]) == 0
                    || strcmp(findData.cFileName, excludes[1]) == 0) {
                    continue;
                }
                DirectoryStack_Push(
                    stack, UtilsFormatStr("%s/%s", root, findData.cFileName));
            } else {
                const char *fileName
                    = UtilsFormatStr("%s/%s", root, findData.cFileName);
                UtilsDebugPrint("File: %s", fileName);
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;
                arr->files = realloc(arr->files, sizeof(struct UtilsFile)
                                                     * (arr->numFiles + 1));
                ZERO_MEMORY_SZ(arr->files[arr->numFiles].name, 256);
                memcpy(arr->files[arr->numFiles].name, fileName,
                       strlen(fileName));
                arr->files[arr->numFiles].size = fileSize.QuadPart;
                arr->numFiles++;
            }
        } while (FindNextFile(file, &findData) != 0);
    }
    DirectoryStack_Destroy(stack);
    return arr;
#else
    UtilsFatalError("FATAL ERROR: Not implemented for current platform");
    return NULL;
#endif
}
