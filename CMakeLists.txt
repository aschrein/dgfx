cmake_minimum_required(VERSION 3.2)

project(main)

add_subdirectory(3rdparty/gfx)
add_subdirectory(3rdparty/pugixml ${CMAKE_CURRENT_BINARY_DIR}/pugixml)

add_compile_definitions(GLM_FORCE_SWIZZLE)
list(APPEND CMAKE_MODULE_PATH "3rdparty/oneapi-tbb/lib/cmake/tbb")
list(APPEND CMAKE_MODULE_PATH "3rdparty/embree/lib/cmake/embree-3.13.5")
set(TBB_DIR "3rdparty/oneapi-tbb/lib/cmake/tbb")
set(embree_DIR "3rdparty/embree/lib/cmake/embree-3.13.5")
FIND_PACKAGE(TBB)
FIND_PACKAGE(embree 3.0)

IF (NOT TBB_FOUND)
        error()
ENDIF()
IF (NOT embree_FOUND)
        error()
ENDIF()

set(SDL2_PATH ${CMAKE_SOURCE_DIR}/3rdparty/SDL2)

function(add_exe target_name source_file)

        add_executable(${target_name}
                src/file_io.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/${source_file})

        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                target_compile_options(${target_name} PUBLIC -Wno-unused-function)
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        endif()

        target_compile_options(${target_name} PRIVATE /bigobj)

        target_compile_definitions(${target_name} PUBLIC DGFX_PATH="")
        set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        target_include_directories(${target_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
        target_link_libraries(${target_name} PUBLIC
                gfx
                pugixml
                embree
                "${SDL2_PATH}/lib/x64/SDL2.lib"
                "${SDL2_PATH}/lib/x64/SDL2main.lib"
        )
        target_compile_features(${target_name} PRIVATE cxx_std_17)
        add_custom_command(
                TARGET ${target_name}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${SDL2_PATH}/lib/x64/SDL2.dll"
                        ${CMAKE_CURRENT_BINARY_DIR}/Debug/SDL2.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                "${SDL2_PATH}/lib/x64/SDL2.dll"
                        ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/SDL2.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                "${SDL2_PATH}/lib/x64/SDL2.dll"
                        ${CMAKE_CURRENT_BINARY_DIR}/Release/SDL2.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                "${SDL2_PATH}/lib/x64/SDL2.dll"
                        ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/SDL2.dll)

        add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxil.dll
                ${CMAKE_CURRENT_BINARY_DIR}/Debug/dxil.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxil.dll
                ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/dxil.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxil.dll
                ${CMAKE_CURRENT_BINARY_DIR}/Release/dxil.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxil.dll
                ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/dxil.dll)

        add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxcompiler.dll
                ${CMAKE_CURRENT_BINARY_DIR}/Debug/dxcompiler.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxcompiler.dll
                ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/dxcompiler.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxcompiler.dll
                ${CMAKE_CURRENT_BINARY_DIR}/Release/dxcompiler.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_DXC_PATH}/bin/x64/dxcompiler.dll
                ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/dxcompiler.dll)

        add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_PIX_PATH}/bin/x64/WinPixEventRuntime.dll
                ${CMAKE_CURRENT_BINARY_DIR}/Debug/WinPixEventRuntime.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_PIX_PATH}/bin/x64/WinPixEventRuntime.dll
                ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/WinPixEventRuntime.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_PIX_PATH}/bin/x64/WinPixEventRuntime.dll
                ${CMAKE_CURRENT_BINARY_DIR}/Release/WinPixEventRuntime.dll
        COMMAND ${CMAKE_COMMAND} -E copy
                ${GFX_PIX_PATH}/bin/x64/WinPixEventRuntime.dll
                ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/WinPixEventRuntime.dll)

        add_custom_command(
                TARGET ${target_name}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/embree3.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/Debug/embree3.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/embree3.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/embree3.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/embree3.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/Release/embree3.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/embree3.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/embree3.dll)
                
        add_custom_command(
                TARGET ${target_name}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/tbb12.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/Debug/tbb12.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/tbb12.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/tbb12.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/tbb12.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/Release/tbb12.dll
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/embree/bin/tbb12.dll
                        ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/tbb12.dll)

endfunction()

add_exe(main main.cpp)
add_exe(jit_test jit_test.cpp)
add_exe(ddgi_experiment ddgi_experiment.cpp)
add_exe(ao_experiment ao_experiment.cpp)
add_exe(specular_experiment specular_experiment.cpp)