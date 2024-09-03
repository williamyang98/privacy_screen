#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "./gl_assert.hpp"
#include "./key_listener.hpp"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <chrono>
#include <thread>
#include <optional>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winuser.h>

struct Shader {
    GLuint id;
    // ~Shader() { GL_ASSERT(glDeleteShader(id)); }
};

struct Program {
    GLuint id;
    // ~Program() { GL_ASSERT(glDeleteProgram(id)); }
};

std::optional<Shader> compile_shader(GLenum type, const char* source) {
    GL_ASSERT(GLuint id = glCreateShader(type));
    GL_ASSERT(glShaderSource(id, 1, &source, nullptr));
    GL_ASSERT(glCompileShader(id));
    GLint result;
    GL_ASSERT(glGetShaderiv(id, GL_COMPILE_STATUS, &result));
    if (result == GL_TRUE) {
        return Shader { id };
    }
    GLint length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        std::vector<char> message;
        message.resize(length+1);
        message[length] = '\0';
        glGetShaderInfoLog(id, length, &length, message.data());
        const char* shader_type = "unknown";
        switch (type) {
        case GL_FRAGMENT_SHADER: shader_type = "fragment"; break;
        case GL_VERTEX_SHADER:   shader_type = "vertex"; break;
        case GL_COMPUTE_SHADER:  shader_type = "compute"; break;
        }
        fprintf(stderr, "[error] failed to compile %s shader\n", shader_type);
        fprintf(stderr, "%s\n", message.data());
    }
    GL_ASSERT(glDeleteShader(id));
    return std::nullopt;
}

std::optional<Program> create_program(const char* vertex_shader_src, const char* fragment_shader_src) {
    auto vertex_shader_opt = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    if (!vertex_shader_opt.has_value()) {
        return std::nullopt;
    }
    auto fragment_shader_opt = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    if (!fragment_shader_opt.has_value()) {
        return std::nullopt;
    }
    auto vertex_shader = vertex_shader_opt.value();
    auto fragment_shader = fragment_shader_opt.value();
    GL_ASSERT(GLuint id = glCreateProgram());
    GL_ASSERT(glAttachShader(id, vertex_shader.id));
    GL_ASSERT(glAttachShader(id, fragment_shader.id));
    GL_ASSERT(glLinkProgram(id));
    GL_ASSERT(glValidateProgram(id));
    GLint status;
    GL_ASSERT(glGetProgramiv(id, GL_VALIDATE_STATUS, &status));
    if (status == GL_TRUE) {
        return Program { id };
    }
    GLint length;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        std::vector<char> message;
        message.resize(length+1);
        message[length] = '\0';
        glGetProgramInfoLog(id, length, &length, message.data());
        fprintf(stderr, "[error] failed to compile gl program\n");
        fprintf(stderr, "%s\n", message.data());
    }
    GL_ASSERT(glDeleteProgram(id));
    return std::nullopt;
}

struct ImageShader {
    Program program;
    GLuint u_texture;
};

std::optional<ImageShader> create_image_shader() {
    static const char* VERTEX_SHADER_SRC = R"GLSL( 
        #version 330 core
        layout(location = 0) in vec2 pos;
        out vec2 texture_pos;
        void main() {
            gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
            texture_pos = pos/2.0 + 0.5;
            texture_pos.y = 1.0-texture_pos.y;
        }
    )GLSL";
    static const char* FRAGMENT_SHADER_SRC = R"GLSL(
        #version 330 core
        layout(location = 0) out vec4 colour;
        uniform sampler2D u_texture;
        in vec2 texture_pos;
        void main() {
            vec2 pos = vec2(gl_FragCoord.x, gl_FragCoord.y);
            colour = texture(u_texture, texture_pos);
        }
    )GLSL";
    auto program_opt = create_program(VERTEX_SHADER_SRC, FRAGMENT_SHADER_SRC);
    if (!program_opt.has_value()) {
        return std::nullopt;
    }
    ImageShader shader;
    shader.program = program_opt.value();
    shader.u_texture = GL_ASSERT(glGetUniformLocation(shader.program.id, "u_texture"));
    return shader;
}

struct Texture {
    GLuint id;
    int width;
    int height;
    int bits_per_pixel;
    // ~Texture() { GL_ASSERT(glDeleteTextures(1, &id)); }
};

std::optional<Texture> load_texture_from_filepath(const char* filepath) {
    FILE* fp = fopen(filepath, "rb");
    if (fp == nullptr) {
        fprintf(stderr, "[error] failed to load image from: %s\n", filepath);
        return std::nullopt;
    }
    int width = 0;
    int height = 0;
    int bits_per_pixel = 0;
    uint8_t* data = stbi_load_from_file(fp, &width, &height, &bits_per_pixel, 4);
    fclose(fp);
    if (data == nullptr) {
        return std::nullopt;
    }

    GLuint id = 0;
    GL_ASSERT(glGenTextures(1, &id)); 
    GL_ASSERT(glBindTexture(GL_TEXTURE_2D, id));
    GL_ASSERT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_ASSERT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_ASSERT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_ASSERT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_ASSERT(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
    stbi_image_free(data);
    return Texture { id, width, height, bits_per_pixel };
}

struct Vertex {
    float x;
    float y;
};

struct SquareMesh {
    GLuint index_buffer;
    GLuint vertex_buffer;
    void bind() {
        // Create a new vertex array for each opengl context since it's not shareable
        GLuint vertex_array;
        GL_ASSERT(glGenVertexArrays(1, &vertex_array));
        GL_ASSERT(glBindVertexArray(vertex_array));
        GL_ASSERT(glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer));
        {
            constexpr int index = 0;
            constexpr int count = 2;
            constexpr int stride = sizeof(Vertex);
            constexpr bool is_normalised = false;
            const GLuint offset = 0;
            GL_ASSERT(glVertexAttribPointer(index, count, GL_FLOAT, is_normalised, stride, (const void*)(offset)));
            GL_ASSERT(glEnableVertexAttribArray(index));
        }
        GL_ASSERT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    }
};

SquareMesh create_square_mesh() {
    const static std::array<GLuint,6> INDEX_BUFFER = {0,1,3,1,2,3};
    const static std::array<Vertex,4> VERTEX_BUFFER = {
        Vertex {-1.0f, 1.0f},
        Vertex { 1.0f, 1.0f},
        Vertex { 1.0f,-1.0f},
        Vertex {-1.0f,-1.0f},
    };
    SquareMesh mesh;
    GL_ASSERT(glGenBuffers(1, &mesh.index_buffer));
    GL_ASSERT(glGenBuffers(1, &mesh.vertex_buffer));

    GL_ASSERT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.index_buffer));
    GL_ASSERT(glBufferData(GL_ELEMENT_ARRAY_BUFFER, INDEX_BUFFER.size()*sizeof(GLuint), INDEX_BUFFER.data(), GL_STATIC_DRAW));

    GL_ASSERT(glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer));
    GL_ASSERT(glBufferData(
        GL_ARRAY_BUFFER, 
        sizeof(Vertex)*VERTEX_BUFFER.size(), reinterpret_cast<const void*>(VERTEX_BUFFER.data()), 
        GL_STATIC_DRAW
    ));
    return mesh;
}

int main(int argc, char** argv) {
    util::init_global_keyboard_listener();
    if (!glfwInit()) {
        fprintf(stderr, "[error] failed to initialise glfw\n");
        return 1;
    }
    // create window for each monitor
    int total_monitors = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&total_monitors);
    if (monitors == nullptr || total_monitors <= 0) {
        fprintf(stderr, "[error] failed to get monitors\n");
        return 1;
    }
    std::vector<GLFWwindow*> windows;
    GLFWwindow* primary_window = nullptr;
    for (int i = 0; i < total_monitors; i++) {
        GLFWmonitor* monitor = monitors[i];
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
        GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Privacy Screen", nullptr, primary_window);
        if (window == nullptr) {
            fprintf(stderr, "[error] failed to create glfw window on monitor %d\n", i);
            return 1;
        }
        // first monitor is primary window
        if (i == 0) {
            glfwMakeContextCurrent(window);
            // setup glew
            glewExperimental = true;
            const GLenum err_glew_init = glewInit();
            if (err_glew_init != GLEW_OK) {
                fprintf(stderr, "[error] glew initialisation error: %s\n", glewGetErrorString(err_glew_init));
                return 1;
            }
            primary_window = window;
        }
        int x = 0;
        int y = 0;
        glfwGetMonitorPos(monitor, &x, &y);
        glfwSetWindowPos(window, x, y);
        // glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        glfwMakeContextCurrent(window);
        // glfwSwapInterval(1);
        windows.push_back(window);
    }
    // create opengl program
    auto shader_opt = create_image_shader();
    if (!shader_opt.has_value()) {
        fprintf(stderr, "[error] failed to create image shader\n");
        return 1;
    }
    auto shader = shader_opt.value();

    std::vector<Texture> textures;
    const std::array<const char*,1> filepaths = { "./images/monitor_0.png" };
    for (const char* filepath: filepaths) {
        auto image_opt = load_texture_from_filepath(filepath);
        if (!image_opt.has_value()) {
            fprintf(stderr, "[warn] failed to load image: %s\n", filepath);
        }
        auto image = image_opt.value();
        textures.push_back(image);
    }
    if (textures.size() > 0) {
        printf("[info] loaded %zu images\n", textures.size());
    } else {
        fprintf(stderr, "[error] no images were able to be loaded\n");
        return 1;
    }
    auto square_mesh = create_square_mesh();

    volatile bool show_windows = false;
    util::attach_global_keyboard_listener(VK_F8, [&show_windows](util::event_code event) {
        if (event == WM_KEYDOWN) {
            show_windows = !show_windows;
        }
    });

    // render to multiple monitors
    while (true) {
        const bool should_show = show_windows;
        bool should_close = false;
        for (size_t i = 0; i < windows.size(); i++) {
            GLFWwindow* window = windows[i];
            glfwMakeContextCurrent(window);
            if (glfwWindowShouldClose(window)) {
                should_close = true;
                continue;
            }
            if (should_show) {
                glfwShowWindow(window);
            } else {
                glfwHideWindow(window);
                continue;
            }
            GL_ASSERT(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
            {
                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                GL_ASSERT(glViewport(0, 0, width, height));
            }
            GL_ASSERT(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
            GL_ASSERT(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
            GL_ASSERT(glDisable(GL_CULL_FACE));
            GL_ASSERT(glEnable(GL_BLEND));
            GL_ASSERT(glEnable(GL_DEPTH_TEST));
            GL_ASSERT(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA));
            GL_ASSERT(glUseProgram(shader.program.id));
            square_mesh.bind();
            constexpr int texture_slot = 0;
            GL_ASSERT(glActiveTexture(GL_TEXTURE0+texture_slot));
            const auto& texture = textures[i % textures.size()];
            glBindTexture(GL_TEXTURE_2D, texture.id);
            GL_ASSERT(glUniform1i(shader.u_texture, texture_slot));
            GL_ASSERT(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
            glfwSwapBuffers(window);
        }
        if (should_close) {
            break;
        }
        if (should_show) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        glfwPollEvents();
    }
    for (auto* window: windows) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
 
    return 0;
}
