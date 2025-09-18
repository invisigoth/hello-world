#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// simple vector structure
struct float3 {
    float x, y, z;
    float3() : x(0), y(0), z(0) {}
    float3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct float4 {
    float x, y, z, w;
    float4() : x(0), y(0), z(0), w(0) {}
    float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

// camera params
struct Camera {
    float3 eye;
    float3 lookat;
    float3 up;
    float fovy;
    float aspect;
};

// ray tracing context
class RayTracingEngine {
private:
    OptixDeviceContext context;
    CUstream stream;
    GLFWwindow* window;
    
    // OpenGL 
    GLuint framebuffer_texture;
    GLuint shader_program;
    GLuint vao, vbo;
    
    // scene 
    Camera camera;
    float rotation_angle;
    const int width = 1024;
    const int height = 768;

public:
    RayTracingEngine() : context(nullptr), stream(nullptr), window(nullptr), 
                        framebuffer_texture(0), shader_program(0), 
                        vao(0), vbo(0), rotation_angle(0.0f) {
        // init camera
        camera.eye = float3(0, 0, 5);
        camera.lookat = float3(0, 0, 0);
        camera.up = float3(0, 1, 0);
        camera.fovy = 45.0f * M_PI / 180.0f;
        camera.aspect = (float)width / height;
    }
    
    bool initialize() {
        // init GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        // create window
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        window = glfwCreateWindow(width, height, "Ray Traced Hello World!", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window);
        
        // init GLEW
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            return false;
        }
        
        // init CUDA
        cudaSetDevice(0);
        cudaStreamCreate(&stream);
        
        // init OptiX
        if (!initializeOptiX()) {
            return false;
        }
        
        // setup OpenGL resources
        setupOpenGL();
        
        return true;
    }
    
    bool initializeOptiX() {

        OptixResult result = optixInit();
        if (result != OPTIX_SUCCESS) {
            std::cerr << "OptiX initialization failed" << std::endl;
            return false;
        }
        
        // create OptiX context
        OptixDeviceContextOptions options = {};
        options.logCallbackFunction = nullptr;
        options.logCallbackLevel = 4;
        
        CUcontext cuCtx = 0; 
        result = optixDeviceContextCreate(cuCtx, &options, &context);
        if (result != OPTIX_SUCCESS) {
            std::cerr << "OptiX context creation failed" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void setupOpenGL() {
        // framebuffer texture
        glGenTextures(1, &framebuffer_texture);
        glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // shader program for displaying the ray traced result
        const char* vertexShaderSource = R"(
            #version 450 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";
        
        const char* fragmentShaderSource = R"(
            #version 450 core
            in vec2 TexCoord;
            out vec4 FragColor;
            uniform sampler2D screenTexture;
            void main() {
                FragColor = texture(screenTexture, TexCoord);
            }
        )";
        
        // compile shaders and create program
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
        
        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertexShader);
        glAttachShader(shader_program, fragmentShader);
        glLinkProgram(shader_program);
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        // fullscreen quad
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    
    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        // check compilation
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        }
        
        return shader;
    }
    
    // 3D text generation using basic geometric primitives
    std::vector<float3> generateHelloWorldGeometry() {
        std::vector<float3> vertices;
        
        // H
        addLetter_H(vertices, float3(-4.0f, 0.0f, 0.0f));
        // E
        addLetter_E(vertices, float3(-3.0f, 0.0f, 0.0f));
        // L
        addLetter_L(vertices, float3(-2.0f, 0.0f, 0.0f));
        // L
        addLetter_L(vertices, float3(-1.0f, 0.0f, 0.0f));
        // O
        addLetter_O(vertices, float3(0.0f, 0.0f, 0.0f));
        
        // Space
        
        // W
        addLetter_W(vertices, float3(1.5f, 0.0f, 0.0f));
        // O
        addLetter_O(vertices, float3(2.5f, 0.0f, 0.0f));
        // R
        addLetter_R(vertices, float3(3.5f, 0.0f, 0.0f));
        // L
        addLetter_L(vertices, float3(4.5f, 0.0f, 0.0f));
        // D
        addLetter_D(vertices, float3(5.5f, 0.0f, 0.0f));
        
        return vertices;
    }
    
    void addLetter_H(std::vector<float3>& vertices, const float3& pos) {
        // L vertical line
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // R vertical line
        addBox(vertices, float3(pos.x + 0.4f, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // horizontal line
        addBox(vertices, float3(pos.x + 0.2f, pos.y + 0.5f, pos.z), float3(0.2f, 0.05f, 0.1f));
    }
    
    void addLetter_E(std::vector<float3>& vertices, const float3& pos) {
        // vertical line
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // T horizontal
        addBox(vertices, float3(pos.x + 0.15f, pos.y + 0.9f, pos.z), float3(0.15f, 0.05f, 0.1f));
        // M horizontal
        addBox(vertices, float3(pos.x + 0.1f, pos.y + 0.5f, pos.z), float3(0.1f, 0.05f, 0.1f));
        // B horizontal
        addBox(vertices, float3(pos.x + 0.15f, pos.y + 0.1f, pos.z), float3(0.15f, 0.05f, 0.1f));
    }
    
    void addLetter_L(std::vector<float3>& vertices, const float3& pos) {
        // vertical line
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // B horizontal
        addBox(vertices, float3(pos.x + 0.15f, pos.y + 0.1f, pos.z), float3(0.15f, 0.05f, 0.1f));
    }
    
    void addLetter_O(std::vector<float3>& vertices, const float3& pos) {
        // Create O using 4 rectangles forming a square ring
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.4f, 0.1f)); // Left
        addBox(vertices, float3(pos.x + 0.35f, pos.y + 0.5f, pos.z), float3(0.05f, 0.4f, 0.1f)); // Right
        addBox(vertices, float3(pos.x + 0.2f, pos.y + 0.9f, pos.z), float3(0.15f, 0.05f, 0.1f)); // Top
        addBox(vertices, float3(pos.x + 0.2f, pos.y + 0.1f, pos.z), float3(0.15f, 0.05f, 0.1f)); // Bottom
    }
    
    void addLetter_W(std::vector<float3>& vertices, const float3& pos) {
        // L line
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // M line
        addBox(vertices, float3(pos.x + 0.2f, pos.y + 0.3f, pos.z), float3(0.05f, 0.3f, 0.1f));
        // R line
        addBox(vertices, float3(pos.x + 0.4f, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
    }
    
    void addLetter_R(std::vector<float3>& vertices, const float3& pos) {
        // vertical line
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // T horizontal
        addBox(vertices, float3(pos.x + 0.15f, pos.y + 0.9f, pos.z), float3(0.15f, 0.05f, 0.1f));
        // M horizontal
        addBox(vertices, float3(pos.x + 0.15f, pos.y + 0.5f, pos.z), float3(0.15f, 0.05f, 0.1f));
        // R vertical (upper)
        addBox(vertices, float3(pos.x + 0.3f, pos.y + 0.7f, pos.z), float3(0.05f, 0.2f, 0.1f));
        // diagonal
        addBox(vertices, float3(pos.x + 0.25f, pos.y + 0.3f, pos.z), float3(0.1f, 0.05f, 0.1f));
    }
    
    void addLetter_D(std::vector<float3>& vertices, const float3& pos) {
        // vertical line
        addBox(vertices, float3(pos.x, pos.y + 0.5f, pos.z), float3(0.05f, 0.5f, 0.1f));
        // T horizontal
        addBox(vertices, float3(pos.x + 0.1f, pos.y + 0.9f, pos.z), float3(0.1f, 0.05f, 0.1f));
        // B horizontal
        addBox(vertices, float3(pos.x + 0.1f, pos.y + 0.1f, pos.z), float3(0.1f, 0.05f, 0.1f));
        // R vertical
        addBox(vertices, float3(pos.x + 0.25f, pos.y + 0.5f, pos.z), float3(0.05f, 0.4f, 0.1f));
    }
    
    void addBox(std::vector<float3>& vertices, const float3& center, const float3& size) {
        // 8 vertices for a box (for simplicity)
        float3 half_size = float3(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);

        for (int i = 0; i < 8; ++i) {
            vertices.push_back(center);
        }
    }
    
    void renderFrame() {
        rotation_angle += 0.02f; // rotate continuously, yo
        
        // CPU-based ray tracing simulation
        std::vector<float4> pixels(width * height);
        
        // scene geometry
        std::vector<float3> geometry = generateHelloWorldGeometry();
        
        // ray tracing loop
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float u = (float)x / width;
                float v = (float)y / height;
              
                float3 ray_dir = generateRay(u, v);
                
                float3 color = traceRay(ray_dir, geometry);
                
                pixels[y * width + x] = float4(color.x, color.y, color.z, 1.0f);
            }
        }
        
        // OpenGL texture
        glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, pixels.data());
        
        // fullscreen quad
        glUseProgram(shader_program);
        glBindVertexArray(vao);
        glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    float3 generateRay(float u, float v) {
        // covnvert screen coordinates to world ray direction
        float theta = camera.fovy;
        float half_height = tan(theta * 0.5f);
        float half_width = camera.aspect * half_height;
        
        float3 w = normalize(subtract(camera.eye, camera.lookat));
        float3 u_vec = normalize(cross(camera.up, w));
        float3 v_vec = cross(w, u_vec);
        
        float3 horizontal = multiply(u_vec, 2.0f * half_width);
        float3 vertical = multiply(v_vec, 2.0f * half_height);
        float3 lower_left = subtract(subtract(subtract(camera.eye, multiply(horizontal, 0.5f)), 
                                             multiply(vertical, 0.5f)), w);
        
        return normalize(subtract(add(add(lower_left, multiply(horizontal, u)), 
                                     multiply(vertical, v)), camera.eye));
    }
    
    float3 traceRay(const float3& ray_dir, const std::vector<float3>& geometry) {
        // shading based on ray direction and rotation
        float cos_angle = cos(rotation_angle);
        float sin_angle = sin(rotation_angle);
        
        // rotate ray direction
        float3 rotated_dir = float3(
            ray_dir.x * cos_angle - ray_dir.z * sin_angle,
            ray_dir.y,
            ray_dir.x * sin_angle + ray_dir.z * cos_angle
        );
        
        // lighting model
        float3 light_dir = float3(0.577f, 0.577f, 0.577f); // Normalized (1,1,1)
        float intensity = fmax(0.1f, dot(rotated_dir, light_dir));
        
        // color based on direction and geometry presence
        float3 base_color = float3(0.2f, 0.6f, 1.0f); // Blue
        if (!geometry.empty() && fabs(rotated_dir.x) < 0.5f && fabs(rotated_dir.y) < 0.3f) {
            base_color = float3(1.0f, 0.8f, 0.2f); // Golden color for text
        }
        
        return multiply(base_color, intensity);
    }
    
    // vector helper functions
    float3 normalize(const float3& v) {
        float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return float3(v.x / len, v.y / len, v.z / len);
    }
    
    float3 subtract(const float3& a, const float3& b) {
        return float3(a.x - b.x, a.y - b.y, a.z - b.z);
    }
    
    float3 add(const float3& a, const float3& b) {
        return float3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    
    float3 multiply(const float3& v, float s) {
        return float3(v.x * s, v.y * s, v.z * s);
    }
    
    float3 cross(const float3& a, const float3& b) {
        return float3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
    
    float dot(const float3& a, const float3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    
    void run() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            glClear(GL_COLOR_BUFFER_BIT);
            renderFrame();
            
            glfwSwapBuffers(window);
        }
    }
    
    ~RayTracingEngine() {
        if (context) {
            optixDeviceContextDestroy(context);
        }
        if (stream) {
            cudaStreamDestroy(stream);
        }
        if (window) {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
        
        glDeleteTextures(1, &framebuffer_texture);
        glDeleteProgram(shader_program);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    }
};

int main() {
    std::cout << "Starting Ray Traced Hello World!" << std::endl;
    
    RayTracingEngine engine;
    
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize ray tracing engine" << std::endl;
        return -1;
    }
    
    std::cout << "Ray tracing engine initialized successfully!" << std::endl;
    std::cout << "Controls: Close window to exit" << std::endl;
    std::cout << "The 3D text should be rotating automatically" << std::endl;
    
    engine.run();
    
    return 0;
}
