#ifndef DEBUG_H
#define DEBUG_H

#ifdef _DEBUG
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
void DebugPrint(const char* format, ...) {
	char buf[4096];
	va_list val;
	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);
	OutputDebugStringA(buf);
}
#else
#define DebugPrint(x) 
#endif

#endif
