#include "glfw_engine.hpp"

#include <cstdio>
#include <stdexcept>

namespace engine {

GLFWContext::GLFWContext() {
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}
}

GLFWContext::~GLFWContext() {
	glfwTerminate();
}

Window::Window(const WindowConfig &config) {
	if (config.width <= 0 || config.height <= 0 || config.title.empty()) {
		throw std::invalid_argument("window dimensions and title must be valid");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
	// glfwWindowHintString(GLFW_WAYLAND_APP_ID, "Engine");
	glfwWindowHintString(GLFW_WAYLAND_APP_ID, "make-me-float");
	window_ = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
	if (window_ == nullptr) {
		throw std::runtime_error("failed to create GLFW window");
	}
}

Window::~Window() {
	if (window_ != nullptr) {
		glfwDestroyWindow(window_);
	}
}

// // Move semantics for Window that enforce exclusice ownership, this makes it easuier and safer to move windows around eg. in a vector of windows, or to return a window from a function without copying it
// Move constructor
Window::Window(Window &&other) noexcept : window_(other.window_) {
	other.window_ = nullptr;
}
// Move assignment operator
Window &Window::operator=(Window &&other) noexcept {
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

} // namespace engine
