export JAVA_HOME=/usr/local/buildtools/java/jdk
javac -g HelloWorld.java
javah HelloWorld
gcc -I"${JAVA_HOME}/include" -I"${JAVA_HOME}/include/linux" -g -fPIC -shared native_lib.c -o native_lib.so
