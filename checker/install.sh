rm -rf build && mkdir build && cd build && cmake ..
make || exit 1
make install
