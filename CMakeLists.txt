cmake_minimum_required(VERSION 3.22)
project(CalmSenseBackend CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SmartSpectra CONFIG REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(calmsense_backend main.cpp)
target_link_libraries(calmsense_backend PRIVATE SmartSpectra::SDK ${OpenCV_LIBS})
