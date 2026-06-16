if [ ! -d build ];then
    mkdir build
else
    rm -rf build/*
fi

cd build
qmake ../hexwalk/hexwalk.pro
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
cd ..
