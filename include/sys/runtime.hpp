#ifndef RUNTIME_HPP
#define RUNTIME_HPP

extern "C" {
    struct atexit_func_entry {
        void (*destructor_func)(void *);
        void *obj;
        void *dso_handle;
    };

    int __cxa_atexit(void (*f)(void *), void *obj, void *dso);
    void __cxa_finalize(void *f);
}

#endif