#ifndef __KWMACRO_H__
#define __KWMACRO_H__

//	199711L (C++98 or C++03)
//	201103L (C++11)
//	201402L (C++14)
//	201703L (C++17)
//	202002L (C++20)

#if __cplusplus >= 202002L
#include <format>
#include <chrono>
#include <ctime>
#include <string>

#ifndef WARN
#define WARN(formatstring, ...) \
{\
std::chrono::time_point now = std::chrono::system_clock::now(); \
std::chrono::time_point now2 = std::chrono::current_zone()->to_local(std::chrono::system_clock::now()); \
fprintf(stderr, "%s %s(%05d) : " formatstring "\n", std::format("{0:%Y-%m-%d %H:%M:%S}", now2).substr(0,23).c_str(), __FILE__,__LINE__,__VA_ARGS__);\
}
#endif

#ifndef WARN0
#define WARN0(formatstring) \
{\
std::chrono::time_point now = std::chrono::system_clock::now(); \
std::chrono::time_point now2 = std::chrono::current_zone()->to_local(std::chrono::system_clock::now()); \
fprintf(stderr, "%s %s(%05d) : " formatstring "\n", std::format("{0:%Y-%m-%d %H:%M:%S}", now2).substr(0,23).c_str(), __FILE__,__LINE__ );\
}
#endif

#ifndef WARN1
#define WARN1(formatstring, ...) \
{\
std::chrono::time_point now = std::chrono::system_clock::now(); \
std::chrono::time_point now2 = std::chrono::current_zone()->to_local(std::chrono::system_clock::now()); \
fprintf(stderr, "%s %s(%05d) : " formatstring , std::format("{0:%Y-%m-%d %H:%M:%S}", now2).substr(0,23).c_str(), __FILE__,__LINE__,__VA_ARGS__);\
}
#endif

#ifdef KW_DEBUG
#ifndef DEBUG
#define DEBUG(formatstring, ...) \
{\
std::chrono::time_point now = std::chrono::system_clock::now(); \
std::chrono::time_point now2 = std::chrono::current_zone()->to_local(std::chrono::system_clock::now()); \
fprintf(stderr, "%s %s(%05d) : " formatstring "\n", std::format("{0:%Y-%m-%d %H:%M:%S}", now2).substr(0,23).c_str(), __FILE__,__LINE__,__VA_ARGS__);\
}
#endif
#ifndef DEBUG0
#define DEBUG0(formatstring) \
{\
std::chrono::time_point now = std::chrono::system_clock::now(); \
std::chrono::time_point now2 = std::chrono::current_zone()->to_local(std::chrono::system_clock::now()); \
fprintf(stderr, "%s %s(%05d) : " formatstring "\n", std::format("{0:%Y-%m-%d %H:%M:%S}", now2).substr(0,23).c_str(), __FILE__,__LINE__ );\
}
#endif

#else
#undef DEBUG0
#undef DEBUG
#define DEBUG0( fmt ) {}
#define DEBUG( fmt, ...) {}
#endif

#else

#include <sys/time.h>

#ifndef WARN
#define WARN(formatstring, ...) \
{\
struct timeval tv; time_t nowtime; struct tm *nowtm; char tmbuf[64];\
gettimeofday(&tv, NULL); nowtime=tv.tv_sec; nowtm = localtime(&nowtime);strftime(tmbuf,sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);\
fprintf(stderr, "%s.%03ld %s(%05d) : " formatstring "\n", tmbuf,tv.tv_usec/1000, __FILE__,__LINE__,__VA_ARGS__);\
}
#endif

#ifndef WARN0
#define WARN0(formatstring) \
{\
struct timeval tv; time_t nowtime; struct tm *nowtm; char tmbuf[64];\
gettimeofday(&tv, NULL); nowtime=tv.tv_sec; nowtm = localtime(&nowtime);strftime(tmbuf,sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);\
fprintf(stderr, "%s.%03ld %s(%05d) : " formatstring "\n", tmbuf,tv.tv_usec/1000, __FILE__,__LINE__ );\
}
#endif

#ifndef WARN1
#define WARN1(formatstring, ...) \
{\
struct timeval tv; time_t nowtime; struct tm *nowtm; char tmbuf[64];\
gettimeofday(&tv, NULL); nowtime=tv.tv_sec; nowtm = localtime(&nowtime);strftime(tmbuf,sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);\
fprintf(stderr, "%s.%03ld %s(%05d) : " formatstring , tmbuf,tv.tv_usec/1000, __FILE__,__LINE__,__VA_ARGS__);\
}
#endif

#ifdef KW_DEBUG
#ifndef DEBUG0
#define DEBUG0( fmt ) {fprintf(stderr, "%s(%04d): " fmt "\n", __FILE__,__LINE__ ); fflush(stderr);}
#endif
#ifndef DEBUG
#define DEBUG( fmt, ...) {fprintf(stderr, "%s(%04d): " fmt "\n", __FILE__,__LINE__, __VA_ARGS__ ); fflush(stderr);}
#endif
#else
#undef DEBUG0
#undef DEBUG
#define DEBUG0( fmt ) {}
#define DEBUG( fmt, ...) {}
#endif

#endif


#ifndef ERR_NULL_CHECK
#define ERR_NULL_CHECK( X ) \
if (! X ) { WARN0( "ERR_NULL_CHECK : " #X ); throw std::runtime_error( #X " is NULL" ); }
#endif

#endif
