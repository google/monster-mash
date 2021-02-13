FROM emscripten/emsdk

# Patching sdl2_ttf.py from latest git commit;
# https://github.com/emscripten-core/emscripten/commit/0aec639126c5274236c40351f77063b8addc85cc#diff-de4f4c81202cdb643f091b729a3dfcce46b59884ad174a0b83e25c3b2627029c
COPY ./sdl2_ttf.py /emsdk/upstream/emscripten/tools/ports/sdl2_ttf.py

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
