#pragma once

#include <vulkan/vulkan.h>

#include <iostream>
#include <functional>

class RenderApplication {
public:
	void run();
private:
	void initWindow();
	bool initVulkan();
	void mainLoop();
	void cleanup();
};
