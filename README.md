# Monster Mash: New Sketch-Based Modeling and Animation Tool

## Introduction

This is the open-source version of Monster Mash.

Monster Mash is a new sketch-based modeling and animation tool that allows you to quickly sketch a character, inflate it into 3D, and promptly animate it. You can perform all interactions in the sketching plane. No 3D manipulation is required.

The web demo (http://MonsterMash.zone) and its source code that is available here accompanies a paper **Dvorožňák et al.: Monster Mash: A Single-View Approach to Casual 3D Modeling and Animation** published in ACM Transactions on Graphics 39(6):214 and presented at SIGGRAPH Asia 2020 conference. (See the [project page](https://dcgi.fel.cvut.cz/home/sykorad/monster_mash) for more details.)

The demo uses a combination of web technologies (mainly for UI) and C++ code.

Disclaimer: This is not an officially supported Google product.

## License

The source code in the "src" directory is licensed under Apache License, Version 2.0. See the LICENSE file for more details. Note that third party code located in the "third_party" directory may be licensed under more restrictive licenses.

## Building

This project uses CMake (https://cmake.org) for building. Some third party libraries are not part of this repository and must be installed in advance:
* SDL library v. 2.0 (https://www.libsdl.org).
* Triangle library (https://www.cs.cmu.edu/~quake/triangle.html): Before compiling the project, please download the source code of the library and place it in the "third_party/triangle" directory. Please check the license of the Triangle library before doing so.

You can build the complete web application using emscripten (https://emscripten.org/), or a simplified desktop version (i.e., only a canvas without a web-based UI) using clang (https://clang.llvm.org/) or gcc (https://gcc.gnu.org/).

### Building on Linux

#### Instructions for building on Ubuntu 20.04

* Download and install the necessary dependencies by running:
  ```
  sudo apt-get install build-essential cmake libsdl2-dev wget unzip git python3
  ```
  *(You can remove `python3` if you only want the desktop version.)*
* Clone this repository into your home directory and change directory to the repo:
  ```
  git clone https://github.com/google/monster-mash.git ~/monster-mash && cd ~/monster-mash
  ```
* Download the Triangle library and unzip it into the appropriate directory:
  ```
  wget http://www.netlib.org/voronoi/triangle.zip && unzip triangle.zip -d third_party/triangle
  ```
* *(Skip if you only want the desktop version.)* Download and install emscripten by following instructions on https://emscripten.org/docs/getting_started/downloads.html
* Create a directory for building and change to it:
  ```
  mkdir -p ./build/Release && cd ./build/Release
  ```
* Build the project:
  * For the complete web version, build the project using emscripten:
    ```
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=PATH_TO_EMSDK/upstream/emscripten/emcc -DCMAKE_CXX_COMPILER=PATH_TO_EMSDK/upstream/emscripten/em++ ../../src && make
    ```
    (Replace PATH_TO_EMSDK with the path to your emsdk directory.)
  * For the desktop version, build the project using clang/gcc:
    ```
    cmake -DCMAKE_BUILD_TYPE=Release ../../src && make
    ```
