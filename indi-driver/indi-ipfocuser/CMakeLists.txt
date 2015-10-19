cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_ipfocuser CXX C)

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set (VERSION_MAJOR 0)
set (VERSION_MINOR 2)
 
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})


################ Roll Off ################
set(ipfocuser_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/ipfocuser.cpp
   )

add_executable(indi_ipfocuser ${ipfocuser_SRCS})
target_link_libraries(indi_ipfocuser ${INDI_DRIVER_LIBRARIES} curl)
install(TARGETS indi_ipfocuser RUNTIME DESTINATION bin )
install(FILES indi_ipfocuser.xml DESTINATION ${INDI_DATA_DIR})

