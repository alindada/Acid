#include "FileObserver.hpp"

namespace acid {
FileObserver::FileObserver(std::filesystem::path path, const Time &delay) :
	m_path(std::move(path)),
	m_delay(delay),
	m_running(true),
	m_thread(&FileObserver::QueueLoop, this) {
	DoWithFilesInPath([this](const std::filesystem::path &file) {
		m_paths[file.string()] = std::filesystem::last_write_time(file);
	});
}

FileObserver::~FileObserver() {
	if (m_thread.joinable()) {
		m_running = false;
		m_thread.join();
	}
}

void FileObserver::DoWithFilesInPath(const std::function<void(std::filesystem::path)> &f) const {
	if (!std::filesystem::is_directory(m_path)) {
		f(m_path);
		return;
	}
	for (auto &file : std::filesystem::recursive_directory_iterator(m_path)) {
		f(file.path());
	}
}

void FileObserver::QueueLoop() {
	while (m_running) {
		// Wait for "delay" milliseconds
		std::this_thread::sleep_for(std::chrono::microseconds(m_delay));

		// Check if one of the old files was erased
		for (auto it = m_paths.begin(); it != m_paths.end();) {
			if (!std::filesystem::exists(it->first)) {
				m_onChange(std::filesystem::path(it->first), Status::Erased);
				it = m_paths.erase(it);
				continue;
			}

			++it;
		}
		
		// Check if a file was created or modified
		DoWithFilesInPath([&](const std::filesystem::path &file) {
			auto lastWriteTime = std::filesystem::last_write_time(file);

			// File creation
			if (!Contains(file.string())) {
				// File modification
				m_paths[file.string()] = lastWriteTime;
				m_onChange(file.string(), Status::Created);
			} else {
				if (m_paths[file.string()] != lastWriteTime) {
					m_paths[file.string()] = lastWriteTime;
					m_onChange(file.string(), Status::Modified);
				}
			}
		});
	}
}

bool FileObserver::Contains(const std::string &key) const {
	// TODO C++20: Remove method
	auto el = m_paths.find(key);
	return el != m_paths.end();
}
}
