#include "./gl_assert.hpp"
#include <stdio.h>
#include <GL/glew.h>

bool gl_clear_errors(const char *func, const char *file, int line) {
    bool has_error = false;
    while (GLenum error = glGetError()) {
        if (error == GL_NO_ERROR) {
            break;
        }
        fprintf(stderr, "[error] gl error at %s@%s:%d (%s)\n", func, file, line, gluErrorString(error));
        has_error = true;
    }
    return has_error;
}
