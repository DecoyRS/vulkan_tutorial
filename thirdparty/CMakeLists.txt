# Configuring GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)

set_target_properties(glfw PROPERTIES FOLDER "Thirdparty/GLFW")
set_target_properties(uninstall PROPERTIES FOLDER "Thirdparty/GLFW")

# Configuring GLM

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)

add_subdirectory(glm)

set_target_properties(glm_shared PROPERTIES FOLDER "Thirdparty/GLM")
set_target_properties(glm_static PROPERTIES FOLDER "Thirdparty/GLM")