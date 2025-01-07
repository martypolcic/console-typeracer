#!/bin/bash


mkdir build
cd build
cmake ..


cd client
make

cd ../server
make
