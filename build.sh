export JAVA_HOME=/usr/local/buildtools/java/jdk
javac -g HelloWorld.java
javah HelloWorld
g++ -I"${JAVA_HOME}/include" -I"${JAVA_HOME}/include/linux" --std=c++11 -g -fPIC -shared native_lib.cpp -o native_lib.so
