#pragma once
#include <cstdint>
#include <string>

namespace xuid
{
	typedef uint8_t uxuid8_t;
	typedef int8_t xuid8_t;
	typedef char* xuid_s_t;
	typedef uint16_t uxuid16_t;
	typedef int16_t xuid16_t;
	typedef uint32_t uxuid32_t;
	typedef int32_t xuid32_t;
	typedef uint64_t uxuid64_t;
	typedef int64_t xuid64_t;
	typedef uint64_t xuid;
	typedef float f32xuid_t;
	typedef double f64xuid_t;
	typedef std::string xuid_str_t;
	typedef bool bxuid_t;
	typedef void xuid_none_t;
}

using namespace xuid;
using namespace std;

#define XUID_ITER(start, cond, end) for(start; cond; end)
#define xuid_true true
#define xuid_false false
#define xuid_null NULL
#define xuid_buf_min 256