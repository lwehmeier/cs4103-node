cmake_minimum_required(VERSION 3.10)
project(cs4103)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
add_subdirectory(boost-cmake)


add_executable(cs4103 main.cpp networkParser.cpp networkParser.h connectionManager.cpp connectionManager.h networkMessage.cpp networkMessage.h logging.cpp logging.h election.h election.cpp main.h lockManager.cpp lockManager.h)


target_link_libraries(cs4103 PUBLIC Boost::filesystem)
target_link_libraries(cs4103 PUBLIC Boost::boost)
target_link_libraries(cs4103 PUBLIC Boost::system)
target_link_libraries(cs4103 PUBLIC Boost::graph)
target_link_libraries(cs4103 PUBLIC Boost::coroutine)
target_link_libraries(cs4103 PUBLIC Boost::log)