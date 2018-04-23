cmake_minimum_required(VERSION 3.10)
project(cs4103)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
add_subdirectory(boost-cmake)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp)

SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -rdynamic " )
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include
        ${CMAKE_CURRENT_LIST_DIR}/inc
)


add_executable(cs4103 src/main.cpp src/networkParser.cpp src/connectionManager.cpp src/networkMessage.cpp src/logging.cpp src/election.cpp src/lockManager.cpp src/visitorAccessMgr.cpp src/clientHandler.cpp)


target_link_libraries(cs4103 PUBLIC Boost::filesystem)
target_link_libraries(cs4103 PUBLIC Boost::boost)
target_link_libraries(cs4103 PUBLIC Boost::system)
target_link_libraries(cs4103 PUBLIC Boost::graph)
target_link_libraries(cs4103 PUBLIC Boost::coroutine)
target_link_libraries(cs4103 PUBLIC Boost::log
        SQLiteCpp
        sqlite3
        pthread
        dl
        )