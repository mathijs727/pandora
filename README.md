# pandora

## Prerequisites
Pandora uses the following third-party libraries:
* Boost
* The Guideline Support Library
* Intel Threaded Building BLocks
* Google Test (for pandoraTest only)

In addition Atlas, the real-time viewer, requires:
* GLFW3
* GLEW
* OpenGL

Most dependencies are shipped with the project as submodules so the user does not need to do a thing.
TBB is shipped as precompiled binaries as provided by Intel from the TBB Github page. On Linux/macOS systems the user is required to install GLEW (binaries are provided for Windows). Also, the user is responsible for installing Boost.

### Windows
Download and install the Boost binaries from:
<http://www.boost.org/users/download/>

### Ubuntu
Install Boost binaries using the package manager:  
```sudo apt install libboost-all-dev```  
__OR__  
Build the latest version of Boost from source ([boost manual](https://github.com/boostorg/boost/wiki/Getting-Started)):
```
git clone --recursive https://github.com/boostorg/boost.git
cd boost
git checkout develop # or whatever branch you want to use
.\bootstrap
.\b2 headers
```

Install GLEW:  
```sudo apt install libglew-dev```


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

To reduce compile times, use the Ninja build system and enable multithreading (replace "<yourcorecounthere>" by the number of build threads that you want to use):
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