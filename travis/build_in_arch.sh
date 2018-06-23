#sudo git clone git@github.com:openexr/openexr.git
#cd openexr
#sudo mkdir build
#cd build
#cmake -GNinja ../
#ninja
#cd ../../
sudo cp -u ./travis/openexr_patches/ImathMatrix.h /usr/include/OpenEXR/
sudo cp -u ./travis/openexr_patches/ImathVec.h /usr/include/OpenEXR/

sudo mkdir build
cd build
sudo cmake -GNinja -DBUILD_TESTS=ON -DBUILD_ATLAS=OFF ../
sudo ninja
ctest --output-on-failure
cd ../
sudo rm -rf build