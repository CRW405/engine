#include <oxide/tools/log.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace oxide {
namespace {

std::string_view baseName(std::string_view path) noexcept {
	const std::size_t separator = path.find_last_of("/\\");
	return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

#if OXIDE_ENABLE_LOGGING
bool colorsEnabled() {
	static const bool enabled = [] {
		if (std::getenv("NO_COLOR") != nullptr) {
			return false;
		}
#if defined(_WIN32)
		return ::_isatty(::_fileno(stderr)) != 0;
#else
		return ::isatty(::fileno(stderr)) != 0;
#endif
	}();
	return enabled;
}

const char *levelName(LogLevel level) noexcept {
	switch (level) {
	case LogLevel::Info:
		return "INFO";
	case LogLevel::Warning:
		return "WARNING";
	case LogLevel::Error:
		return "ERROR";
	case LogLevel::Fatal:
		return "FATAL";
	}
	return "UNKNOWN";
}

const char *levelColor(LogLevel level) noexcept {
	switch (level) {
	case LogLevel::Info:
		return "\033[36m";
	case LogLevel::Warning:
		return "\033[33m";
	case LogLevel::Error:
		return "\033[31m";
	case LogLevel::Fatal:
		return "\033[1;91m";
	}
	return "";
}
#endif

} // namespace

void logMessage(
    LogLevel                    level,
    std::string_view            message,
    const std::source_location &location) {
#if !OXIDE_ENABLE_LOGGING
	(void)level;
	(void)message;
	(void)location;
#else
	const bool color = colorsEnabled();
	if (color) {
		std::cerr << levelColor(level);
	}
	std::cerr << "[oxide][" << levelName(level) << ']';
	if (color) {
		std::cerr << "\033[0m";
	}
	std::cerr << ' ';

	if (location.file_name() != nullptr && location.file_name()[0] != '\0') {
		std::cerr << baseName(location.file_name()) << ':' << location.line() << ": ";
	}
	std::cerr << message << '\n';
#endif
}

LogLine::~LogLine() {
	logMessage(level_, stream_.str(), location_);
}

std::string locatedMessage(
    std::string_view            message,
    const std::source_location &location) {
	std::string result;
	if (location.file_name() != nullptr && location.file_name()[0] != '\0') {
		result += baseName(location.file_name());
		result += ':';
		result += std::to_string(location.line());
		result += ": ";
	}
	result += message;
	return result;
}

std::runtime_error runtimeError(
    std::string                 message,
    const std::source_location &location) {
	return std::runtime_error(locatedMessage(message, location));
}

std::invalid_argument invalidArgument(
    std::string                 message,
    const std::source_location &location) {
	return std::invalid_argument(locatedMessage(message, location));
}

std::out_of_range outOfRange(
    std::string                 message,
    const std::source_location &location) {
	return std::out_of_range(locatedMessage(message, location));
}

} // namespace oxide
