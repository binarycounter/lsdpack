# lsdpack

Records LSDj songs for use in stand-alone Game Boy ROMs. (e.g. your own games, demos, music albums...)

## Building

Requires CMake and a C++ compiler. Exact build steps are platform dependent - see [Running CMake](https://cmake.org/runningcmake/)

## Usage

All songs in the .sav must first be prepared so that they are eventually stopped with the HFF command. Then, place your .sav and .gb file in the same directory and run e.g. `./lsdjpack.exe lsdj.gb` to record the songs to `lsdj.s`. The Game Boy player ROM can now be built using RGBDS:

    rgbasm -o boot.o boot.s
    rgbasm -o player.o player.s
    rgbasm -o lsdj.o lsdj.s
    rgblink -o player.gb boot.o player.o lsdj.o
    rgbfix -v -m 0x19 -p 0 player.gb