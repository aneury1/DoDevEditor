cmake_minimum_required(VERSION 3.16)
project(GdbExample LANGUAGES CXX)
add_executable(app main.cpp)
target_compile_features(app PRIVATE cxx_std_17)
