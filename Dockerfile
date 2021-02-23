FROM ubuntu:20.04

RUN apt-get update -y
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata
RUN apt-get install -y build-essential cmake libsdl2-dev wget unzip git python3

# Install emscripten
RUN git clone https://github.com/emscripten-core/emsdk.git
WORKDIR /emsdk
RUN ./emsdk install latest
RUN ./emsdk activate latest
RUN /bin/bash -c "source ./emsdk_env.sh"

COPY . /usr/src/monster-mash
WORKDIR /usr/src/monster-mash

# Download triangle library.
RUN wget http://www.netlib.org/voronoi/triangle.zip triangle.zip
RUN unzip triangle.zip -d third_party/triangle

# Compile.
RUN mkdir -p ./build/Release
WORKDIR /usr/src/monster-mash/build/Release
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/emsdk/upstream/emscripten/emcc -DCMAKE_CXX_COMPILER=/emsdk/upstream/emscripten/em++ ../../src && make

CMD ["/bin/bash"]
