if [ ! -d build ];then
    mkdir build
else
    rm -rf build/*
fi

cd build
qmake-qt5 ../hexwalk/hexwalk.pro
make
cd ..
