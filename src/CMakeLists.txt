# Copyright 2020-2021 Google LLC
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     https://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.5)

project(monstermash LANGUAGES CXX C)

set(SOURCES
    main.cpp
    defengarapl.cpp
    cpanim.cpp
    loadsave.cpp
    reconstruction.cpp
    mainwindow.cpp
    mypainter.cpp
    mywindow.cpp
    def3dsdl.cpp
    exportgltf.cpp
    ../third_party/ir3d-utils/regionToMesh.cpp
    ../third_party/ir3d-utils/MeshBuilder.cpp
    ../third_party/image/imageReadWrite.cpp
    ../third_party/image/imageScale.cpp
    ../third_party/image/imageUtils.cpp
    ../third_party/miscutils/def3d.cpp
    ../third_party/miscutils/mesh3d.cpp
    ../third_party/miscutils/fsutils.cpp
    ../third_party/miscutils/opengltools.cpp
    ../third_party/miscutils/camera.cpp
    ../third_party/triangle/triangle.c
    ../third_party/zip-mod/zip.c
    ../third_party/SDL2_gfx-mod/SDL2_gfxPrimitives-mod.c
    ../third_party/SDL2_gfx/SDL2_rotozoom.c
    ../third_party/tinygltf/tinygltf.cpp
)

set(HEADERS
    commonStructs.h
    defengarapl.h
    cpanim.h
    loadsave.h
    reconstruction.h
    mainwindow.h
    mypainter.h
    mywindow.h
    def3dsdl.h
    macros.h
    exportgltf.h
    ../third_party/ir3d-utils/regionToMesh.h
    ../third_party/image/image.h
    ../third_party/image/imageReadWrite.hpp
    ../third_party/image/imageUtils.h
    ../third_party/miscutils/fsutils.h
    ../third_party/miscutils/macros.h
    ../third_party/miscutils/camera.h
    ../third_party/miscutils/def3d.h
    ../third_party/miscutils/mesh3d.h
    ../third_party/miscutils/opengltools.h
)

if (CMAKE_CXX_COMPILER MATCHES "em\\+\\+$")
    message("Compiling for WebAssembly/Emscripten")
    set(COMPILER_FLAGS -sWASM=1 -sUSE_SDL=2 -sUSE_SDL_TTF=2)
    set(COMPILER_FLAGS ${COMPILER_FLAGS} -sALLOW_MEMORY_GROWTH=1)
    set(COMPILER_FLAGS ${COMPILER_FLAGS} -sMINIFY_HTML=0)
    set(COMPILER_FLAGS ${COMPILER_FLAGS} -sENVIRONMENT=web)
    set(COMPILER_FLAGS_OPENGL -sFULL_ES2=1)
    set(LINKER_FLAGS_OPENGL ${COMPILER_FLAGS_OPENGL})
#    set(LINKER_FLAGS ${LINKER_FLAGS} ${COMPILER_FLAGS} "--preload-file ${CMAKE_SOURCE_DIR}/../data/examples@/tmp/examples")
    set(LINKER_FLAGS ${LINKER_FLAGS} ${COMPILER_FLAGS} "--preload-file ${CMAKE_SOURCE_DIR}/../data/shaders@../../data/shaders")
    set(LINKER_FLAGS ${LINKER_FLAGS} "--shell-file ${CMAKE_SOURCE_DIR}/ui/myshell.html")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

    # Dirty hack: FE_UNDERFLOW is missing in emscripten (https://github.com/emscripten-core/emscripten/commit/3d6116296e8bc2ed601f109edcbbd9e33ed3b705)
    set(DEFINES ${DEFINES} FE_UNDERFLOW=16)
else()
    message("Compiling for Linux")
    # SDL2
    find_package(SDL2 REQUIRED)
    set(LINKER_FLAGS ${LINKER_FLAGS} -lpthread)

    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

    set(DEFINES
#        ENABLE_DEBUG_CMD_IMG
#        ENABLE_DEBUG_CMD_IR
        ENABLE_DEBUG_CMD_MM
        )

    find_package(OpenGL REQUIRED)
endif()

set(COMPILER_FLAGS ${COMPILER_FLAGS}
    -Wno-sign-compare -Werror=return-type -Wno-narrowing
)

set(LINKER_FLAGS ${LINKER_FLAGS}
    ${SDL2_LIBRARIES}
)

execute_process(COMMAND date +%y%m%d
                OUTPUT_VARIABLE APP_VERSION_VAR
                OUTPUT_STRIP_TRAILING_WHITESPACE)

message("MM version: ${APP_VERSION_VAR}")

set(DEFINES ${DEFINES}
    IMAGE_READ_WRITE
    IMAGE_MINIZ_EXTERNAL
    IMAGE_SCALE
    TRILIBRARY
    NO_TIMER
    IGL_PARALLEL_FOR_FORCE_SERIAL
    EIGEN_MPL2_ONLY
    EIGEN_SPARSESOLVERBASE_H
    _SDL2_gfxPrimitives_h
    TINYGLTF_NO_STB_IMAGE
    TINYGLTF_NO_STB_IMAGE_WRITE
    TINYGLTF_NO_EXTERNAL_IMAGE
    TINYGLTF_USE_CPP14
    APP_VERSION=${APP_VERSION_VAR}
)

set(DEFINES_OPENGL
  USE_SDL_GLES2
  USE_OPENGL_FOR_DRAWING_INSTEAD_OF_SDL
)

set(INCLUDEPATH
    ../third_party
    ../third_party/eigen3-mod
    ../third_party/eigen3
    ../third_party/libigl/include
    ../third_party/stb
    ../third_party/triangle
    ../third_party/zip-mod
    ../third_party/SDL2_gfx
    ../third_party/SDL2_gfx-mod
    ../third_party/tinygltf
    ${SDL2_INCLUDE_DIRS}
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(monstermash ${SOURCES} ${HEADERS})
target_compile_definitions(monstermash PRIVATE ${DEFINES} ${DEFINES_OPENGL} ${DEFINES_EMSCRIPTEN})
target_include_directories(monstermash PRIVATE ${INCLUDEPATH})
target_link_libraries(monstermash ${LINKER_FLAGS} ${LINKER_FLAGS_OPENGL} ${OPENGL_LIBRARIES})
target_compile_options(monstermash PRIVATE ${COMPILER_FLAGS} ${COMPILER_FLAGS_OPENGL})
