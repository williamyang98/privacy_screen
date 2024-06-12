#pragma once
#include <assert.h>

#define GL_ASSERT(func) \
    gl_clear_errors(#func, __FILE__, __LINE__);\
    func;\
    assert(!gl_clear_errors(#func, __FILE__, __LINE__))

bool gl_clear_errors(const char *func, const char *file, int line);
