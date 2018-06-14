ls
sudo mkdir build
cd build
sudo cmake -GNinja -DBUILD_TESTS=ON -DBUILD_ATLAS=OFF ../
sudo ninja
ctest --output-on-failure
cd ../
sudo rm -rf build