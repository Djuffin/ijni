if [ -z "$JAVA_HOME" ]; then
    export JAVA_HOME=/usr/local/buildtools/java/jdk
fi
javac -g HelloWorld.java
javah HelloWorld
g++ -I"${JAVA_HOME}/include" -I"${JAVA_HOME}/include/linux" -fno-omit-frame-pointer `llvm-config --cxxflags --ldflags --libs engine` -g -fPIC -O3 -shared native_lib.cpp -o native_lib.so
