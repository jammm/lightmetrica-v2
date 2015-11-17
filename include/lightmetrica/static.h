/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/macros.h>

#include <iostream>
#include <string>
#include <memory>
#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#endif

LM_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

class DynamicLibrary
{
public:

    bool Load(const std::string& path)
    {
        handle = LoadLibraryA(path.c_str());
        if (!handle)
        {
            std::cerr << "Failed to load library " << std::endl;
            std::cerr << std::to_string(GetLastError()) << std::endl;
            return false;
        }
        return true;
    }

    void* GetFuncPointer(const std::string& symbol) const
    {
        void* address = GetProcAddress(handle, symbol.c_str());
        if (address == nullptr)
        {
            std::cerr << "Failed to get address of '" + symbol + "' : " + std::to_string(GetLastError()) << std::endl;
            return nullptr;
        }

        return address;
    }

public:

    HMODULE handle;

};

// --------------------------------------------------------------------------------


struct InternalPolicy {};
struct ExternalPolicy {};

#ifdef LM_EXPORTS
using InitPolicy = InternalPolicy;
#else
using InitPolicy = ExternalPolicy;
#endif

/*
    Static initialization.
    
    Performs a static initialization using
    the technique to combine static member function and singleton
    using this technique, the process in the constructor is called once.

    \tparam InitPolicy
        Controls initialization policy.
        If the StaticInit class is specified with `ExternalPolicy`,
        the initialization is dedicated to the load from external source.
*/
template <typename InitPolicy>
class StaticInit;

template <>
class StaticInit<InternalPolicy>
{
public:

    static auto Instance() -> StaticInit&
    {
        static StaticInit inst;
        return inst;
    }

    StaticInit() {}

};

template <>
class StaticInit<ExternalPolicy>
{
public:

    static auto Instance() -> StaticInit&
    {
        static StaticInit inst;
        return inst;
    }

    StaticInit()
        : lib(new DynamicLibrary)
    {
        // Load DLL
        // Assume in the dynamic library is in the same directory as executable
        // TODO: search from search paths.
        if (!lib->Load("liblightmetrica.dll"))
        {
            // This is fatal error, the application should exit immediately
            std::exit(EXIT_FAILURE);
        }
    }

    auto Library() const -> const DynamicLibrary*
    {
        return lib.get();
    }

private:

    std::unique_ptr<DynamicLibrary> lib;

};


namespace
{
    struct StaticInitReg { static const StaticInit<InitPolicy>& reg; };
    const StaticInit<InitPolicy>& StaticInitReg::reg = StaticInit<InitPolicy>::Instance();
}

// --------------------------------------------------------------------------------

#ifdef LM_EXPORTS
    #define LM_EXPORTED_F(Func, ...) return Func(__VA_ARGS__);
#else
    #define LM_EXPORTED_F(Func, ...) \
        using FuncPtrType = decltype(&Func); \
        static auto func = []() -> FuncPtrType { \
            const auto* lib = StaticInit<ExternalPolicy>::Instance().Library(); \
            const auto* f = static_cast<FuncPtrType>(lib->GetFuncPointer(#Func)); \
            if (!f) std::exit(EXIT_FAILURE); \
            return f; \
        }(); \
        return func(__VA_ARGS__);
#endif

// --------------------------------------------------------------------------------

extern "C" LM_PUBLIC_API auto StaticFuncTest_Func1() -> int;
extern "C" LM_PUBLIC_API auto StaticFuncTest_Func2(int v1, int v2) -> int;

class StaticFuncTest
{
private:
    LM_DISABLE_CONSTRUCT(StaticFuncTest);

public:
    static auto Func1() -> int               { LM_EXPORTED_F(StaticFuncTest_Func1); }
    static auto Func2(int v1, int v2) -> int { LM_EXPORTED_F(StaticFuncTest_Func2, v1, v2); }
};

LM_NAMESPACE_END
