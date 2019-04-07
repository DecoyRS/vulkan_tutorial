find_package(Vulkan REQUIRED)

set(SOURCE
    main.cpp
    render_application.cpp
    render_application.hpp
)
add_executable(vulkan_tutorial ${SOURCE})
set_target_properties(vulkan_tutorial PROPERTIES FOLDER "Apps")
target_link_libraries(vulkan_tutorial Vulkan::Vulkan glfw glm)
target_include_directories(vulkan_tutorial PRIVATE ${CMAKE_CURRENT_LIST_DIR})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${SOURCE}
)

file(GLOB STL_FILES
    "${CMAKE_CURRENT_LIST_DIR}/STL/*"
)
foreach(loop_var ${STL_FILES})
    configure_file(${loop_var} ${loop_var} COPYONLY)
endforeach(loop_var)