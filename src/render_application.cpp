#include <render_application.hpp>

void RenderApplication::run() {
	if(!initVulkan()) return;
	mainLoop();
	cleanup();
}

bool RenderApplication::initVulkan()
{
	return true;
}

void RenderApplication::mainLoop()
{
	return;
}

void RenderApplication::cleanup()
{
	return;
}

