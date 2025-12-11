cmake_minimum_required(VERSION 3.15)
project(HwBrowserPresence)

set(CMAKE_CXX_STANDARD 17)

# Main executable
add_executable(hwbrowserpresence-bridge bridge.cpp)

if(WIN32)
    # Windows: use provided lib and header
    target_include_directories(hwbrowserpresence-bridge PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(hwbrowserpresence-bridge 
        ${CMAKE_SOURCE_DIR}/rpc-windows.lib
        ws2_32 
        shell32
    )
    set_target_properties(hwbrowserpresence-bridge PROPERTIES 
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "hwbrowserpresence-windows-bridge"
    )
    # Add icon resource
    target_sources(hwbrowserpresence-bridge PRIVATE icon.rc)
else()
    # Linux: use provided .so and header
    target_include_directories(hwbrowserpresence-bridge PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(hwbrowserpresence-bridge 
        ${CMAKE_SOURCE_DIR}/rpc-linux.so
        pthread
        dl
    )
    set_target_properties(hwbrowserpresence-bridge PROPERTIES 
        OUTPUT_NAME "hwbrowserpresence-linux-bridge"
    )
endif()
