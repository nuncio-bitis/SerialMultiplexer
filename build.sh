OLDPWD=${PWD}
cd build
cmake ..
make -j 8
cd ${OLDPWD}
