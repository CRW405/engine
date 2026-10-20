#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace engine {

struct WindowConfig {
	int         width     = 500;
	int         height    = 500;
	std::string title     = "Engine";
	bool        resizable = true;
};

class GLFWContext {
  public:
	explicit GLFWContext();
	~GLFWContext();

	GLFWContext(const GLFWContext &)            = delete;
	GLFWContext &operator=(const GLFWContext &) = delete;
};

class Window {
  public:
	explicit Window(const WindowConfig &config);
	~Window();

	Window(const Window &)            = delete;
	Window &operator=(const Window &) = delete;

	Window(Window &&) noexcept;
	Window &operator=(Window &&) noexcept;

	[[nodiscard]] GLFWwindow *handle() const noexcept { return window_; }
	[[nodiscard]] int         width() const noexcept;
	[[nodiscard]] int         height() const noexcept;
	[[nodiscard]] bool        shouldClose() const noexcept;
	void                      pollEvents() const noexcept;
	[[nodiscard]] double      time() const noexcept;
	[[nodiscard]] bool        keyPressed(int key) const noexcept;

  private:
	GLFWwindow *window_ = nullptr;
};

}
