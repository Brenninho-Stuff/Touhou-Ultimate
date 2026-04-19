# Instalar Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Compilar project.cpp para WebAssembly
emcc project.cpp \
    -o touhou_ultimate.js \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS='["_main"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=256MB \
    -lembind \
    -O2 \
    --preload-file assets@/ \
    -s USE_SDL=2 \
    -s USE_SDL_IMAGE=2 \
    -s USE_SDL_TTF=2
