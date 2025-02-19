#include "DynamicLibrary.hpp"
#include <debug/Logger.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

DynamicLibrary::DynamicLibrary(const std::string& path) {
#ifdef _WIN32
    handle = LoadLibraryA(path.c_str());
#else
    handle = dlopen(path.c_str(), RTLD_LAZY);
#endif
    if (!handle) {
        char buffer[0xFF]{};
        sprintf(buffer, "Library load failed -> %s", path.c_str());
        throw std::runtime_error(buffer);
    }
}

DynamicLibrary::~DynamicLibrary() {
    if (handle) {
    #ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
    #else
        dlclose(handle);
    #endif
    }
}