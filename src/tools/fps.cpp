#include "fps.hpp"

#include <cstdint>
#include <cstdio>

namespace tools {

FPSCounter::FPSCounter(double start_time) : report_time_(start_time) {}

void FPSCounter::update(double current_time) {
	++frames_since_report_;
	const double elapsed = current_time - report_time_;
	if (elapsed < 1.0) {
		return;
	}

	const double fps = static_cast<double>(frames_since_report_) / elapsed;
	std::printf("\r\033[KFPS: %.1f", fps);
	std::fflush(stdout);

	frames_since_report_ = 0;
	report_time_         = current_time;
}

void FPSCounter::finish() const {
	std::printf("\n");
}

}
