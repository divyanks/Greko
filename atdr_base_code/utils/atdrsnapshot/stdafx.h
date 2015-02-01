// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <shlwapi.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>

// TODO: reference additional headers your program requires here
#pragma comment (lib, "VssApi.lib")

// TODO: reference additional headers your program requires here
#define HILONG(ll) (ll >> 32 & LONG_MAX)
#define LOLONG(ll) ((long)(ll))

// Helper macros to print a GUID using printf-style formatting
#define WSTR_GUID_FMT  _T("{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}")

#define GUID_PRINTF_ARG( X )                                \
    (X).Data1,                                              \
    (X).Data2,                                              \
    (X).Data3,                                              \
    (X).Data4[0], (X).Data4[1], (X).Data4[2], (X).Data4[3], \
    (X).Data4[4], (X).Data4[5], (X).Data4[6], (X).Data4[7]




