#include <render_application.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <filesystem>

using namespace std::filesystem;
using namespace std;

int main() {
	RenderApplication renderApplication;

	renderApplication.run();

	return EXIT_SUCCESS;
}