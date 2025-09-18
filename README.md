# "Hello World!" in 3D, with ray tracing, yo! 
## When Your Normal “Hello World” is Mid… but You Have an RTX

### Dependencies (RHEL 9)
```console
sudo dnf install gcc-c++ cmake glfw-devel glew-devel
```

This is not needed for the CPU and OpenGL version
```console
sudo dnf install cuda-toolkit-12-3
```

```console
sudo dnf install mesa-libGL mesa-libGLU mesa-libGL-devel mesa-dri-drivers
```

```console
sudo dnf install freeglut freeglut-devel
```


### Compile
Optional: test if GLFW is working:
```console
g++ glfw_test.cpp -o glfw_test -lglfw -lGL
```
You should see a blank window when ```glfw_test``` is executed


"Hello World!" in 3D using CPU and OpenGL:
```console
g++ hello_world_3d.cpp -o hello_world_rd -lglfw -lGL -lGLU -lglut
```
