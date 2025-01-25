#pragma once
#include "framework.h"
#include <fstream>
#include <filesystem>

#define PATCH_CONFIG_LOCATION "t7patch.conf"
#define DEBUG_SECURITY 0

struct sec_config
{
	std::filesystem::file_time_type modified;
	uint64_t password_history[2];
	uint64_t password_changed_time;
	uint8_t is_friends_only;
    uint8_t exists;
	char playername[16];

    bool fs_exists(const char* filename);
    bool update_watcher_time(const char* path);
    void loadfrom(const char* path);
    void saveto(const char* path);
};

#define ZBR_PREFIX_BYTE (uint8_t)((security::conf.password_history[1] & 0xFF0000) >> 16)
#define ZBR_PREFIX_BYTE2 (uint8_t)((security::conf.password_history[1] & 0xFF000000) >> 24)

namespace security
{
	extern sec_config conf;
	void init();
	void init_delayed();
	bool handle_exception(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);

    uint64_t canon_hash64(const char* input);
}

#if DEBUG_SECURITY
#define SALOG(...) ALOG(__VA_ARGS__)
#else
#define SALOG(...)
#endif