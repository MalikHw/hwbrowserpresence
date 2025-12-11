cmake_minimum_required(VERSION 3.15)
project(HwBrowserPresence)

set(CMAKE_CXX_STANDARD 17)

# Discord RPC library
add_library(discord-rpc STATIC discord-rpc/src/discord_rpc.cpp discord-rpc/src/rpc_connection.cpp discord-rpc/src/serialization.cpp discord-rpc/src/connection_win.cpp discord-rpc/src/connection_unix.cpp discord-rpc/src/discord_register_win.cpp discord-rpc/src/discord_register_linux.cpp)
target_include_directories(discord-rpc PUBLIC discord-rpc/include)

if(WIN32)
    target_compile_definitions(discord-rpc PRIVATE DISCORD_WINDOWS)
    target_link_libraries(discord-rpc PRIVATE psapi advapi32)
else()
    target_compile_definitions(discord-rpc PRIVATE DISCORD_LINUX)
endif()

# Main executable
add_executable(hwbrowserpresence-bridge bridge.cpp)
target_link_libraries(hwbrowserpresence-bridge discord-rpc)

if(WIN32)
    target_link_libraries(hwbrowserpresence-bridge ws2_32 shell32)
    set_target_properties(hwbrowserpresence-bridge PROPERTIES WIN32_EXECUTABLE TRUE)
    # Add icon resource
    target_sources(hwbrowserpresence-bridge PRIVATE icon.rc)
endif()

# Set output names based on platform
if(WIN32)
    set_target_properties(hwbrowserpresence-bridge PROPERTIES OUTPUT_NAME "hwbrowserpresence-windows-bridge")
else()
    set_target_properties(hwbrowserpresence-bridge PROPERTIES OUTPUT_NAME "hwbrowserpresence-linux-bridge")
endif()
