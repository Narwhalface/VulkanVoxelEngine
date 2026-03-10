#ifndef RENDER_LOOP_HPP
#define RENDER_LOOP_HPP

struct GLFWwindow;
class VulkanApp;

void RenderLoop(VulkanApp& app, GLFWwindow* window);

#endif // RENDER_LOOP_HPP
