FROM emscripten/emsdk:2.0.14


COPY . /usr/src/monster-mash
WORKDIR /usr/src/monster-mash

# Download triangle library.
RUN wget http://www.netlib.org/voronoi/triangle.zip triangle.zip
RUN unzip triangle.zip -d third_party/triangle

# Compile.
RUN pip3 install requests
RUN emcmake cmake src
RUN make

CMD ["/bin/bash"]
