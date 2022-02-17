#define _CRT_SECURE_NO_WARNINGS
#include "log.h"
#include <time.h>
#include <stdio.h>
#include <memory>
#include <stdarg.h>
#include <io.h>
#include <filesystem>
#include <stdio.h>
#include <chrono>
#include <vector>

#if defined(_MSC_VER)
#include <direct.h>
#define GetCurrentDir _getcwd
#define makedir(_path_) _mkdir(_path_)
#elif defined(__unix__)
#include <unistd.h>
#include <sys/stat.h>
#define GetCurrentDir getcwd
#define makedir(_path_) mkdir(_path_, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#else
#endif

rofirger::Log* rofirger::Log::_ptr_log = NULL;
std::mutex rofirger::Log::_mutex_get_instance;
rofirger::Log* rofirger::Log::GetInstance()
{
	if (NULL == rofirger::Log::_ptr_log)
	{
		rofirger::Log::_mutex_get_instance.lock();
		if (NULL == rofirger::Log::_ptr_log)
		{
			_ptr_log = new rofirger::Log();
		}
		rofirger::Log::_mutex_get_instance.unlock();
	}
	return _ptr_log;
}

rofirger::Log::~Log()
{
	if (NULL != rofirger::Log::_ptr_log)
	{
		delete _ptr_log;
	}
}

void rofirger::Log::SetMsgBufferSize(size_t _s_)noexcept
{
	_msg_buffer_size = _s_;
}

void rofirger::Log::SetFolderPath(const char* _folder_path_)noexcept
{
	_folder_path = _folder_path_;
}

bool rofirger::Log::StartLog()
{
	_is_stop = false;
	struct stat info;
	if (_folder_path.empty() || stat(_folder_path.c_str(), &info) != 0)
	{
		char current_folder[256]{ 0 };
		char new_log_folder[256]{ 0 };
		if (NULL == GetCurrentDir(current_folder, 256)) return false;
		sprintf(new_log_folder, "%s\\log\\", current_folder);
		_folder_path = new_log_folder;
		if (stat(new_log_folder, &info) == 0) goto POS_LOG_STARTLOG_FUNC_BREAK_IF;
		if (-1 == makedir(new_log_folder)) return false;
	}
POS_LOG_STARTLOG_FUNC_BREAK_IF:
	time_t now_time = time(NULL);
	struct tm* t = localtime(&now_time);
	char log_file_path[256];
	sprintf(log_file_path, "%s%04d%02d%02d%02d%02d%02d.log", _folder_path.c_str(), t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	_file_path = log_file_path;
	_ptr_file = fopen(log_file_path, "wt+");
	if (NULL == _ptr_file) return false;
	_thread.reset(new std::thread(std::bind(&rofirger::Log::ThreadFunc, this)));
	return true;
}

void rofirger::Log::StopLog()
{
	while (!_queue_log_data.empty()) {}
	_is_stop = true;
	_cv.notify_one();
	_thread->join();
}

void rofirger::Log::AddLog(LOG_LEVEL _log_level_, const char* _file_, size_t _line_num_, const char* _func_sig_, const char* fmt_, ...)
{
	std::vector<char> msg;
	msg.resize(_msg_buffer_size);
	va_list ap;
	va_start(ap, fmt_);
	vsnprintf(msg.data(), msg.size(), fmt_, ap);
	va_end(ap);

	time_t now = time(NULL);
	struct tm* tmstr = localtime(&now);
	char complete_log[10500] = { 0 };
	auto chrono_time_now = std::chrono::system_clock::now();
	auto chrono_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(chrono_time_now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(chrono_time_now.time_since_epoch()).count() * 1000;
	sprintf(complete_log, "[%04d-%02d-%02d %02d:%02d:%02d.%03d][%s][0x%04x][FILE:%s LINE:%d FUNC:%s][MSG:%s]\n",
		tmstr->tm_year + 1900,
		tmstr->tm_mon + 1,
		tmstr->tm_mday,
		tmstr->tm_hour,
		tmstr->tm_min,
		tmstr->tm_sec,
		chrono_msecs,
		_level_str[(int)_log_level_].c_str(),
		std::this_thread::get_id(),
		_file_,
		_line_num_,
		_func_sig_,
		msg.data());

	{
		std::lock_guard<std::mutex> guard(_mutex);
		_queue_log_data.push(complete_log);
	}
	_cv.notify_one();
}

void rofirger::Log::ThreadFunc()
{
	if (NULL == _ptr_file)
	{
		return;
	}
	while (!_is_stop)
	{
		std::unique_lock<std::mutex> lock_(_mutex);
		while (_queue_log_data.empty())
		{
			if (_is_stop) return;
			_cv.wait(lock_);
		}
		CheckFileSize();
		const std::string& str_ = _queue_log_data.front();
		fwrite((void*)str_.c_str(), str_.length(), 1, _ptr_file);
		fflush(_ptr_file);
		_queue_log_data.pop();
	}
}

void rofirger::Log::CheckFileSize()
{
	struct stat info;
	constexpr long log_file_overflow_size = 1024 * 1024;
	if (stat(_file_path.c_str(), &info) == 0)
	{
		_off_t file_size_ = info.st_size; // the number of bytes of the file
		if (file_size_ > log_file_overflow_size)
		{
		POS_LOG_RESTART_MAKE_NEW_FILE:
			time_t now_time = time(NULL);
			struct tm* t = localtime(&now_time);
			char log_file_path[256];
			sprintf(log_file_path, "%s%04d%02d%02d%02d%02d%02d.log", _folder_path.c_str(), t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			_file_path = log_file_path;
			do { _ptr_file = fopen(log_file_path, "wt+"); } while (_ptr_file == NULL);
		}
	}
	else
	{
		goto POS_LOG_RESTART_MAKE_NEW_FILE;
	}
}
