#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace engine {

struct WindowConfig {
	int         width     = 500;
	int         height    = 500;
	std::string title     = "Engine";
	bool        resizable = true;
};

struct MousePosition {
	double x = 0.0;
	double y = 0.0;
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
	[[nodiscard]] bool        keyPressed(int key) noexcept;
	[[nodiscard]] bool        keyReleased(int key) const noexcept;
	[[nodiscard]] bool        mouseButtonDown(int button) const noexcept;
	[[nodiscard]] MousePosition cursorPosition() const noexcept;
	[[nodiscard]] MousePosition consumeScrollDelta() noexcept;

  private:
	static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
	static void scrollCallback(GLFWwindow *window, double x_offset, double y_offset);
	void        handleKeyEvent(int key, int action) noexcept;
	void        handleScrollEvent(double x_offset, double y_offset) noexcept;

	GLFWwindow *window_ = nullptr;
	std::unordered_set<int> pressed_keys_;
	MousePosition           scroll_delta_;

	static std::unordered_map<GLFWwindow *, Window *> instances_;
};

} // namespace engine
