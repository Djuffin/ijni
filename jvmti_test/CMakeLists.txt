cmake_minimum_required(VERSION 3.4.1)
project (tiagent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library( # Specifies the name of the library.
            tiagent

            # Sets the library as a shared library.
            SHARED

            # Provides a relative path to your source file(s).
            agent.cpp )

target_compile_features(tiagent PRIVATE cxx_nullptr)