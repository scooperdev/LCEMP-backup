cmake_minimum_required(VERSION 3.10)
project("LCEMP")

set(LCEMP_WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets" CACHE STRING "The working directory for MinecraftClient")

set(CMAKE_CXX_STANDARD 11)

if(NOT WIN32 AND NOT UNIX)
    message(FATAL_ERROR "Only Windows and Linux are supported")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Sources.cmake")

list(TRANSFORM MINECRAFT_WORLD_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/")

if(UNIX)
    set(BUNDLED_ZLIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/adler32.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/compress.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/crc32.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/deflate.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/inffast.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/inflate.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/inftrees.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/trees.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/uncompr.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/zlib/zutil.c"
    )
    add_library(MinecraftWorld STATIC ${MINECRAFT_WORLD_SOURCES} ${BUNDLED_ZLIB_SOURCES})
else()
    add_library(MinecraftWorld STATIC ${MINECRAFT_WORLD_SOURCES})
endif()

target_include_directories(MinecraftWorld PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World"
        "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
)

if(WIN32)
    target_compile_definitions(MinecraftWorld PRIVATE
            $<$<CONFIG:Debug>:_LARGE_WORLDS;_DEBUG_MENUS_ENABLED;_DEBUG;_LIB;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
            $<$<NOT:$<CONFIG:Debug>>:_LARGE_WORLDS;_LIB;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
    )
endif()

if(MSVC)
    target_compile_options(MinecraftWorld PRIVATE /W3 /MP $<$<CONFIG:Debug>:/MTd> $<$<NOT:$<CONFIG:Debug>>:/MT> /EHsc)
endif()

if(UNIX)
    target_include_directories(MinecraftWorld BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/Linux/stubs"
    )
    target_include_directories(MinecraftWorld PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/Sentient/Include"
    )
    target_compile_options(MinecraftWorld PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/Linux/LinuxCompat.h"
            -Wno-write-strings -Wno-narrowing -Wno-deprecated
    )
    target_compile_definitions(MinecraftWorld PRIVATE
            _LARGE_WORLDS _LIB _WINDOWS64
            _CRT_NON_CONFORMING_SWPRINTFS _CRT_SECURE_NO_WARNINGS
    )
endif()

if(WIN32)
    list(TRANSFORM MINECRAFT_CLIENT_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/")

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_executable(MinecraftClient WIN32 ${MINECRAFT_CLIENT_SOURCES})

    set_property(TARGET MinecraftClient PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${LCEMP_WORKING_DIR}")

    if(MSVC)
        target_compile_options(MinecraftClient PRIVATE /W3 /MP $<$<CONFIG:Debug>:/MTd> $<$<NOT:$<CONFIG:Debug>>:/MT> /EHsc)
    endif()

    target_compile_definitions(MinecraftClient PRIVATE
            $<$<CONFIG:Debug>:_LARGE_WORLDS;_DEBUG_MENUS_ENABLED;_DEBUG;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
            $<$<NOT:$<CONFIG:Debug>>:_LARGE_WORLDS;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
    )

    target_include_directories(MinecraftClient PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/Sentient/Include"
    )

    target_link_libraries(MinecraftClient PRIVATE
            MinecraftWorld
            d3d11
            XInput9_1_0
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggy_w64.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Miles/lib/mss64.lib"
            $<$<CONFIG:Debug>:
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Input_d.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Storage_d.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Profile_d.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Render_PC_d.lib"
            >
            $<$<NOT:$<CONFIG:Debug>>:
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Input_r.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Storage_r.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Profile_r.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Render_PC.lib"
            >
    )
endif()

if(UNIX)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ServerSources.cmake")

    list(TRANSFORM MINECRAFT_SERVER_OWN_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/")
    list(TRANSFORM MINECRAFT_SERVER_SHARED_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/")

    add_executable(MinecraftDedicatedServer
            ${MINECRAFT_SERVER_OWN_SOURCES}
            ${MINECRAFT_SERVER_SHARED_SOURCES}
    )

    target_compile_definitions(MinecraftDedicatedServer PRIVATE
            _DEDICATED_SERVER
            _CONTENT_PACKAGE
            _LARGE_WORLDS
            _WINDOWS64
            WITH_SERVER_CODE
            _CRT_NON_CONFORMING_SWPRINTFS
            _CRT_SECURE_NO_WARNINGS
    )

    target_include_directories(MinecraftDedicatedServer BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/Linux/stubs"
    )
    target_include_directories(MinecraftDedicatedServer PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/Linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/inc"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/Sentient/Include"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
    )

    target_compile_options(MinecraftDedicatedServer PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/Linux/LinuxCompat.h"
            -Wno-write-strings -Wno-narrowing -Wno-deprecated
    )

    find_package(Threads REQUIRED)
    target_link_libraries(MinecraftDedicatedServer PRIVATE
            MinecraftWorld
            Threads::Threads
    )
endif()