# Print "Hello World!" in 3D with ray tracing 
## RHEL 9 w/ RTX 3060

### Features:

- Depends on NVIDIA's OptiX API for hardware-accelerated ray tracing
- Creates geometric letter shapes for "HELLO WORLD"
- The text contineously rotates for your visual pleasure
- Renders at around 50 FPS with proper lighting
- Uses OpenGL for final frame presentation

### Dependencies (RHEL 9)
```console
sudo dnf install gcc-c++ cmake glfw-devel glew-devel
```

### Compile
```console
nvcc -std=c++14 -O3 -o hello_world_rt hello_world_rt.cpp -lglfw -lGLEW -lGL -lcuda -lcudart -I/path/to/optix/include -L/path/to/optix/lib -loptix
```
