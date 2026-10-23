#include "glfw_engine.hpp"

#include <cstdio>
#include <stdexcept>

namespace engine {

// -----------------------------------------------------------------------------
// GLFW lifetime and window management
//
// GLFWContext controls the process-wide GLFW lifetime. Window owns one GLFW
// window and exposes only the operations needed by the renderer and main loop.
// -----------------------------------------------------------------------------
GLFWContext::GLFWContext() {
	// Step 1: initialize GLFW before any window or Vulkan surface is created.
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}
}

GLFWContext::~GLFWContext() {
	glfwTerminate();
}

Window::Window(const WindowConfig &config) {
	// Step 2: reject invalid settings before passing them to GLFW.
	if (config.width <= 0 || config.height <= 0 || config.title.empty()) {
		throw std::invalid_argument("window dimensions and title must be valid");
	}

	// Step 3: create a window with Vulkan presentation handled externally.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHintString(GLFW_WAYLAND_APP_ID, "make-me-float");
	window_ = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
	if (window_ == nullptr) {
		throw std::runtime_error("failed to create GLFW window");
	}
}

Window::~Window() {
	// Destroying the window releases the native handle owned by this wrapper.
	if (window_ != nullptr) {
		glfwDestroyWindow(window_);
	}
}

Window::Window(Window &&other) noexcept : window_(other.window_) {
	other.window_ = nullptr;
}

Window &Window::operator=(Window &&other) noexcept {
	// Move assignment replaces this wrapper's owned native handle.
	if (this != &other) {
		if (window_ != nullptr) {
			glfwDestroyWindow(window_);
		}
		window_       = other.window_;
		other.window_ = nullptr;
	}
	return *this;
}

int Window::width() const noexcept {
	int width  = 0;
	int height = 0;
	if (window_ != nullptr) {
		glfwGetFramebufferSize(window_, &width, &height);
	}
	return width;
}

int Window::height() const noexcept {
	int width  = 0;
	int height = 0;
	if (window_ != nullptr) {
		glfwGetFramebufferSize(window_, &width, &height);
	}
	return height;
}

bool Window::shouldClose() const noexcept {
	return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void Window::pollEvents() const noexcept {
	glfwPollEvents();
}

double Window::time() const noexcept {
	return glfwGetTime();
}

bool Window::keyPressed(int key) const noexcept {
	return window_ != nullptr && glfwGetKey(window_, key) == GLFW_PRESS;
}

}
