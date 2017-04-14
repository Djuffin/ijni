export JAVA_HOME=/usr/local/buildtools/java/jdk
javac -g -Xdiags:verbose  -classpath "asm-debug-all-5.2.jar" Adapt.java
javac -g -Xdiags:verbose  -classpath "asm-debug-all-5.2.jar" ArraySet.java
#java -Xdiag -cp ".:asm-debug-all-5.2.jar"  Adapt
