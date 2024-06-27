#pragma once
#include "pcileech.h"
#ifdef PCILEECHLIB_EXPORTS
#define PCILEECHLIB_API __declspec(dllexport)
#else
#define PCILEECHLIB_API __declspec(dllimport)
#endif

PCILEECHLIB_API int StartDump(OnProgressNotify opn, const char* outPath);
PCILEECHLIB_API void StopDump();