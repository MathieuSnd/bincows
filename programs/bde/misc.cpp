#include <stdlib.h>

void* operator new(size_t sz) noexcept  {
    return malloc(sz);
}


void* operator new[](size_t sz) noexcept {
    return malloc(sz);
}


void operator delete[](void* ptr) noexcept {
    free(ptr);
}

extern "C" void* __dso_handle;

extern "C" int __cxa_atexit(
        void (*func) (void*), void* arg, void* dso_handle)
{
    return 0;
}




