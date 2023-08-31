cmake_minimum_required(VERSION 3.24)
project(ServerNew)

set(CMAKE_CXX_STANDARD 17)

link_libraries(ws2_32 wsock32)

add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

add_executable(Server main.cpp)
