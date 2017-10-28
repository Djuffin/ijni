rm -rf build
mkdir build
cd build
ANDROID_NDK_HOME=~/android-ndk-r15c
cmake -DCMAKE_BUILD_TYPE=Debug -DANDROID_STL=c++_static -DANDROID_PIE=TRUE -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake -DANDROID_ABI=$abi  -GNinja -DANDROID_NATIVE_API_LEVEL=21 ..
ninja