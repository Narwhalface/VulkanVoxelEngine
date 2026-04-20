#ifndef RENDER_LOOP_HPP
#define RENDER_LOOP_HPP

struct GLFWwindow;
class VulkanApp;

/**
 * Runs the main event/render loop until the window closes.
 * @param app Vulkan application instance used to render frames.
 * @param window GLFW window used for event processing and close checks.
 * @return No return value.
 */
void RenderLoop(VulkanApp& app, GLFWwindow* window);

#endif // RENDER_LOOP_HPP
