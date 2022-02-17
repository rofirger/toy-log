// based on the C++11 standard
// the Log doesn't support wide character path. 
#ifndef _ROFIRGER_LOG_H_
#define _ROFIRGER_LOG_H_
#include <string>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
namespace rofirger
{
	typedef enum LOG_LEVEL
	{
		LOG_LEVEL_INFO = 0x00,
		LOG_LEVEL_WARNING = 0x01,
		LOG_LEVEL_ERROR = 0x02,
		LOG_LEVEL_FATAL = 0x03
	}LOG_LEVEL;
#define add_log(_log_level_, ...) Log::GetInstance()->AddLog(_log_level_,__FILE__,__LINE__,__FUNCSIG__,__VA_ARGS__)
	class Log
	{
	public:
		static Log* GetInstance();
		void SetMsgBufferSize(size_t _s_)noexcept;
		void SetFolderPath(const char* _folder_path_)noexcept;
		bool StartLog();
		void StopLog();
		void AddLog(LOG_LEVEL _log_level_, const char* _file_, size_t _line_num_, const char* _func_sig_, const char* fmt_, ...);
	private:
		Log() = default;
		~Log();
		Log(const Log& _log_) = delete;
		Log& operator=(const Log& _log_) = delete;
		void ThreadFunc();
		void CheckFileSize();
	private:
		std::string _level_str[4]{ "INFO","WARNING","ERROR","FATAL" };
		static Log* _ptr_log;
		std::mutex _mutex;
		static std::mutex _mutex_get_instance;
		std::condition_variable _cv;
		std::string _folder_path;
		std::string _file_path;
		FILE* _ptr_file = nullptr;
		std::queue<std::string> _queue_log_data;
		std::shared_ptr<std::thread>    _thread;
		bool _is_stop;
		size_t _msg_buffer_size = 1024; // default:1024, the size can be reassigned.
	};
}
#endif