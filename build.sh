mkdir Debug
cd Debug
cmake -DCMAKE_BUILD_TYPE=Debug -DHMDF_BENCHMARKS=1 -DHMDF_EXAMPLES=1 -DHMDF_TESTING=1 ..
make
#make install
