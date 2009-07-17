#ifndef AVISYNTHDLL_H
#define AVISYNTHDLL_H

#include "stdio.h"
#include "avisynth.h"

// Tdll from ffdshow
class Tdll {
public:
	bool ok;
	HMODULE hDll;
	Tdll(const char* name, const char *path) {
		if (!(hDll = LoadLibrary(name))) {
			char name1[MAX_PATH], name2[MAX_PATH], name3[MAX_PATH], ext[MAX_PATH];
			_splitpath(name1, NULL, NULL, name1, ext);
			_makepath(name2, NULL, NULL, name1, ext);
			if (!(hDll = LoadLibrary(name2)) && path) {
				sprintf(name3, "%s%s", path, name2);
				hDll = LoadLibrary(name3);
			}
		}
		ok = (hDll != NULL);
	}
	~Tdll() {
		if (hDll) FreeLibrary(hDll);
	}
	void loadFunction(void** func,const char* name) {
		*func = NULL;
		*func = (void*) GetProcAddress(hDll, name);
		ok &= (*func != NULL);
	}
};

class AviSynthDll {
public:
	bool ok;
	AviSynthDll(const char* path) {
		dll = NULL;
		ok = false;
		LoadDll(path);
	}
	~AviSynthDll() {
		if (dll) delete dll;
		dll = NULL;
	}
	IScriptEnvironment* CreateScriptEnvironment() {
		return func( AVISYNTH_INTERFACE_VERSION );
	}
private:
	Tdll* dll;
	IScriptEnvironment* (__stdcall *func)(int version);
	void LoadDll(const char* path) {
		ok = false;
		if (dll) delete dll;
		dll = NULL;
		dll = new Tdll(path, NULL);
		dll->loadFunction((void**) &func, "CreateScriptEnvironment");
		if (dll->ok)
			ok = true;
	}
};

#endif
