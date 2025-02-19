// DynamicLibrary.hpp
#pragma once
#include <string>
#include <stdexcept>
#include <dlfcn.h>

class DynamicLibrary {
public:
    DynamicLibrary(const std::string& path);
    ~DynamicLibrary();

    template <typename T>
    T getSymbol(const std::string& name) {
    #ifdef _WIN32
        auto symbol = GetProcAddress(static_cast<HMODULE>(handle), name.c_str());
    #else
        auto symbol = dlsym(handle, name.c_str());
    #endif
        if (!symbol) {
            throw std::runtime_error("Failed to load symbol: " + name);
        }
        return reinterpret_cast<T>(symbol);
    }

private:
    void* handle = nullptr;
};