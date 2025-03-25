cmake_minimum_required(VERSION 3.10)

project(test)
set(CMAKE_CXX_STANDARD 17)

set(AMF_SDK_DIR ../AMF)
set(AMF_SDK_SOURCES
    ${AMF_SDK_DIR}/amf/public/common/AMFFactory.cpp
    ${AMF_SDK_DIR}/amf/public/common/AMFSTL.cpp
    ${AMF_SDK_DIR}/amf/public/common/DataStreamFactory.cpp
    ${AMF_SDK_DIR}/amf/public/common/DataStreamFile.cpp
    ${AMF_SDK_DIR}/amf/public/common/DataStreamMemory.cpp
    ${AMF_SDK_DIR}/amf/public/common/Thread.cpp
    ${AMF_SDK_DIR}/amf/public/common/TraceAdapter.cpp
    ${AMF_SDK_DIR}/amf/public/common/Windows/ThreadWindows.cpp
)

set(TARGETS test benchmark)

foreach(TARGET ${TARGETS})
    add_executable(
        ${TARGET}
        ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.cpp
        ${AMF_SDK_SOURCES}
    )

    target_include_directories(
        ${TARGET} PRIVATE
        ${AMF_SDK_DIR}/amf
    )
endforeach(TARGET)

install(
    TARGETS ${TARGETS}
    RUNTIME DESTINATION bin
)
