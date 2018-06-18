# Pandora
[![Build Status](https://travis-ci.com/mathijs727/pandora.svg?token=BHkWQ9P5pzBfP88jbtB8&branch=master)](https://travis-ci.com/mathijs727/pandora)  
During the Advanced Graphics course at Utrecht University me and a fellow student build our first (OpenCL) path tracer. Afterwards I did an internship at Disney Animation working on the traversal code of the Hyperion renderer. Pandora is my first personal attempt to build a path tracer from scratch. It is meant as a project for me to learn more about path tracing and everything that comes with it. Furthermore, I will use this project to experiment with build tools.

Currently, the goal is to make Pandora a very scalable path tracer. I want it to support both scene distribution and screen distribution so that it can scale to both high scene sizes and low render times. Additionally, out-of-core rendering might also be a future extension. GPU support is also planned. GPUs are extremely performant, but only if the scene fits in GPU memory. By building a scene distributed renderer I hope to find a use GPUs for rendering very large scenes.

High performance computing is something that has interested me for a long time but I have not had much experience with it at the university. Hopefully, building Pandora will help me self-teach some of these topics. In my future I hope to be working on the technical side of rendering.

## Prerequisites
To build Pandora, CMake, Python and a C++17 compiler are required. The build script is set up such that all third party libraries are downloaded and compiled automatically (using Python). Both Python 2 and 3 are supported.

Pandora uses the following third-party libraries:
 - Guideline Support Library (implemented by Microsoft)
 - Intel Threaded Building Blocks (TBB)
 - Assimp
 - Open Image IO
 - EASTL
 - Google Test (only when testing is enabled)

In addition Atlas, the real-time viewer, requires:
 - GLFW3
 - GLEW
 - OpenGL

## Install
Option 1 (Windows only):  
Open the root folders CMakeLists.txt file in Visual Studio 2017

Option 2:  
Use CMake to generate a build script:
```
git clone git@github.com:mathijs727/pandora.git
cd pandora
mkdir build
cd build
cmake ../
cmake --build .
```

To reduce compile times, use the Ninja build system and enable multithreading (replace `<yourcorecounthere>` by the number of build threads that you want to use):
```
git clone git@github.com:mathijs727/pandora.git
cd pandora
mkdir build
cd build
cmake -G Ninja ../
cmake --build . -- -j<yourcorecounthere>
```  
  
Compile with:  
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-stdlib=libc++ -fsanitize=address" ../
```

TBB_USE_GLIBCXX_VERSION