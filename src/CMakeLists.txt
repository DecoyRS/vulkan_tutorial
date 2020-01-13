find_package(Vulkan REQUIRED)

set(SOURCE
    app.cpp
)

file(GLOB shader_files
    "${CMAKE_CURRENT_LIST_DIR}/shaders/*"
)


find_program(glslang_validator_path
    NAMES glslangValidator
    PATHS ENV VULKAN_SDK
    PATH_SUFFIXES $<IF:$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},"AMD64">,Bin,Bin32>
)

if(glslang_validator_path)
    foreach(shader_src_file ${shader_files})
        get_filename_component(file_name ${shader_src_file} NAME_WE)
        get_filename_component(file_ext ${shader_src_file} EXT)
        string(SUBSTRING ${file_ext} 1 -1 file_ext_no_dot)
        set(shader_name ${file_name}_${file_ext_no_dot})
        set(shader_bin_file_name ${shader_name}.hpp)
        set(shader_bin_folder ${CMAKE_CURRENT_LIST_DIR}/shader_bin)
        set(shader_bin_path ${shader_bin_folder}/${shader_bin_file_name})
        
        add_custom_command(
            OUTPUT ${shader_bin_path}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${shader_bin_folder}"
            COMMAND ${glslang_validator_path} -V ${shader_src_file} --vn ${shader_name} -o ${shader_bin_path}
            DEPENDS ${shader_src_file}
        )
        list(APPEND shader_bin_files ${shader_bin_path})
    endforeach(shader_src_file)
    add_custom_target(compile_shaders
        DEPENDS ${shader_bin_files}
    )
endif()

add_executable(vulkan_tutorial ${SOURCE} ${shader_bin_files} ${shader_files})
set_target_properties(vulkan_tutorial PROPERTIES FOLDER "Apps")
target_link_libraries(vulkan_tutorial Vulkan::Vulkan glfw glm)
target_include_directories(vulkan_tutorial PRIVATE ${CMAKE_CURRENT_LIST_DIR})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${SOURCE}
)

# file(GLOB STL_FILES
#     "${CMAKE_CURRENT_LIST_DIR}/STL/*"
# )
# foreach(loop_var ${STL_FILES})
#     configure_file(${loop_var} ${loop_var} COPYONLY)
# endforeach(loop_var)