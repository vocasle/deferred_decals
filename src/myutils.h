#pragma once

#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
//#include <d3d11.h>
//#include <dinput.h>
#include <stdint.h>

static const int MAX_CONSOLE_LINES = 500;

void UtilsDebugPrint(const char* fmt, ...);

void UtilsFatalError(const char* fmt, ...);

const char* UtilsFormatStr(const char* fmt, ...);

#define UTILS_FATAL_ERROR(msg, ...) UtilsFatalError("ERROR: %s:%d: %s", __FILE__, __LINE__, UtilsFormatStr(msg, __VA_ARGS__))

int UtilsStrFindLastChar(const char* str, const char ch);

void UtilsStrSub(const char* str, uint32_t start, uint32_t end, char out[], uint32_t maxSize);

const char *UtilsGetStrAfterChar(const char *str, const char ch);

unsigned char* UtilsReadData(const char* filepath, unsigned int* bufferSize);

#define ARRAY_COUNT(x) (u32)(sizeof(x) / sizeof(x[0]))

#define ZERO_MEMORY(ptr) memset(ptr, 0, sizeof *(ptr))
#define ZERO_MEMORY_SZ(ptr, sz) memset(ptr, 0, sz)

/* Dynamic Array */

#define DEFINE_ARRAY_TYPE(DataType, ClassSuffix) \
	\
	void Array##ClassSuffix##Resize(struct Array##ClassSuffix* arr, size_t capacity) \
	{ \
		DataType* Data = realloc(arr->Data, capacity * sizeof(DataType)); \
		assert(Data); \
		arr->Data = Data; \
		arr->Capacity = capacity; \
	} \
	\
	void Array##ClassSuffix##PushBack(struct Array##ClassSuffix* arr, DataType v) \
	{ \
		if (!arr->Data || arr->Count + 1 > arr->Capacity) \
		{ \
			size_t size = arr->Data ? arr->Capacity * 2 : 4; \
			Array##ClassSuffix##Resize(arr, size); \
		} \
		arr->Data[arr->Count++]=v; \
	} \
	\
	void Array##ClassSuffix##Free(struct Array##ClassSuffix* arr) \
	{ \
		arr->Count = 0; \
		free(arr->Data); \
	} \
	\
	void Array##ClassSuffix##FreeCustom(struct Array##ClassSuffix* arr, void (*CustomFree)(struct Array##ClassSuffix* arr)) \
	{ \
		for (uint32_t i = 0; i < arr->Count; ++i) \
			CustomFree(&arr->Data[i]); \
		arr->Count = 0; \
		free(arr->Data); \
	}

#define DECLARE_ARRAY_TYPE(DataType, ClassSuffix) \
	struct Array##ClassSuffix \
	{ \
		size_t Count; \
		size_t Capacity; \
		DataType* Data; \
	}; \
	\
	void Array##ClassSuffix##Resize(struct Array##ClassSuffix* arr, size_t capacity); \
	\
	void Array##ClassSuffix##PushBack(struct Array##ClassSuffix* arr, DataType v); \
	\
	void Array##ClassSuffix##Free(struct Array##ClassSuffix* arr); \
	\
	void Array##ClassSuffix##FreeCustom(struct Array##ClassSuffix* arr, void (*CustomFree)(struct Array##ClassSuffix* arr))

#define COM_FREE(This) (This->lpVtbl->Release(This))

#define HR(x) \
	if (FAILED(x)) { \
		UTILS_FATAL_ERROR("%s:%d - Operation failed.", __FILE__, __LINE__); \
	}
