#!/bin/bash

mkdir build
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release
cd build
make -j17 exe # -j arg is max jobs + 1, here configured for 16 compilation threads
cd ..

if [ "$1" == "noiseTool" ]
then
	cd build/resources/FastNoise2/NoiseTool/
	make
	cd ../../..
	cp ./Release/bin/NoiseTool ..
	cd ..
fi

if [ "$1" == "clean" ]
then
	rm -r ./build
fi
