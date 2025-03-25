cmake_minimum_required(VERSION 3.10)

project(test)
set(CMAKE_CXX_STANDARD 17)

set(AMF_SDK_DIR ../AMF)

add_executable(
    ${CMAKE_PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test.cpp
    ${AMF_SDK_DIR}/amf/public/common/AMFFactory.cpp
    ${AMF_SDK_DIR}/amf/public/common/AMFSTL.cpp
    ${AMF_SDK_DIR}/amf/public/common/DataStreamFactory.cpp
    ${AMF_SDK_DIR}/amf/public/common/DataStreamFile.cpp
    ${AMF_SDK_DIR}/amf/public/common/DataStreamMemory.cpp
    ${AMF_SDK_DIR}/amf/public/common/Thread.cpp
    ${AMF_SDK_DIR}/amf/public/common/TraceAdapter.cpp
    ${AMF_SDK_DIR}/amf/public/common/Windows/ThreadWindows.cpp
)

target_include_directories(
    ${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AMF_SDK_DIR}/amf
)

target_link_libraries(
    ${CMAKE_PROJECT_NAME}
)

install(
    TARGETS ${CMAKE_PROJECT_NAME}
    RUNTIME DESTINATION bin
)
