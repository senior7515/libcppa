/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_CONFIG_HPP
#define CPPA_CONFIG_HPP

// detect compiler and set CPPA_DEPRECATED
#if defined(__clang__)
#  define CPPA_CLANG
#  define CPPA_DEPRECATED __attribute__((__deprecated__))
#elif defined(__GNUC__)
#  define CPPA_GCC
#  define CPPA_DEPRECATED __attribute__((__deprecated__))
#elif defined(_MSC_VER)
#  define CPPA_DEPRECATED __declspec(deprecated)
#else
#  define CPPA_DEPRECATED
#endif

// detect OS
#if defined(__APPLE__)
#  define CPPA_MACOS
#  ifndef _GLIBCXX_HAS_GTHREADS
#    define _GLIBCXX_HAS_GTHREADS
#  endif
#elif defined(__linux__)
#  define CPPA_LINUX
#   include <linux/version.h>
#   if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16)
#     define CPPA_POLL_IMPL
#   endif
#elif defined(WIN32)
#  define CPPA_WINDOWS
#else
#  error Plattform and/or compiler not supportet
#endif

#include <memory>
#include <cstdio>
#include <cstdlib>

// import backtrace and backtrace_symbols_fd into cppa::detail
#ifdef CPPA_WINDOWS
#include "cppa/detail/execinfo_windows.hpp"
#else
#include <execinfo.h>
namespace cppa {
namespace detail {
using ::backtrace;
using ::backtrace_symbols_fd;
} // namespace detail
} // namespace cppa
#endif

//
#ifdef CPPA_DEBUG_MODE
#   define CPPA_REQUIRE__(stmt, file, line)                                    \
        printf("%s:%u: requirement failed '%s'\n", file, line, stmt);          \
        {                                                                      \
            void* array[10];                                                   \
            size_t size = ::cppa::detail::backtrace(array, 10);                \
            ::cppa::detail::backtrace_symbols_fd(array, size, 2);              \
        }                                                                      \
        abort()
#   define CPPA_REQUIRE(stmt)                                                  \
        if ((stmt) == false) {                                                 \
            CPPA_REQUIRE__(#stmt, __FILE__, __LINE__);                         \
        }((void) 0)
#else
#   define CPPA_REQUIRE(unused) static_cast<void>(0)
#endif

#define CPPA_CRITICAL__(error, file, line) {                                   \
        printf("%s:%u: critical error: '%s'\n", file, line, error);            \
        exit(7);                                                               \
    } ((void) 0)

#define CPPA_CRITICAL(error) CPPA_CRITICAL__(error, __FILE__, __LINE__)

#ifdef CPPA_WINDOWS
#   include <w32api.h>
#   undef _WIN32_WINNT
#   undef WINVER
#   define _WIN32_WINNT WindowsVista
#   define WINVER WindowsVista
#   include <ws2tcpip.h>
#   include <winsock2.h>
    // remove interface which is defined in rpc.h in files included by
    // windows.h as it clashes with name used in own code
#   undef interface
#else
#   include <unistd.h>
#   include <errno.h>
#endif

namespace cppa {

/**
 * @brief An alternative for the 'missing' @p std::make_unqiue.
 */
template<typename T, typename... Args>
std::unique_ptr<T> create_unique(Args&&... args) {
    return std::unique_ptr<T>{new T(std::forward<Args>(args)...)};
}

// platform-dependent types for sockets and some utility functions
#ifdef CPPA_WINDOWS
    typedef SOCKET native_socket_type;
    typedef const char* setsockopt_ptr;
    typedef const char* socket_send_ptr;
    typedef char* socket_recv_ptr;
    typedef int socklen_t;
    constexpr SOCKET invalid_socket = INVALID_SOCKET;
    inline int last_socket_error() { return WSAGetLastError(); }
    inline bool would_block_or_temporarily_unavailable(int errcode) {
        return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
    }
#else
    typedef int native_socket_type;
    typedef const void* setsockopt_ptr;
    typedef const void* socket_send_ptr;
    typedef void* socket_recv_ptr;
    constexpr int invalid_socket = -1;
    inline void closesocket(native_socket_type fd) { close(fd); }
    inline int last_socket_error() { return errno; }
    inline bool would_block_or_temporarily_unavailable(int errcode) {
        return errcode == EAGAIN || errcode == EWOULDBLOCK;
    }
#endif

} // namespace cppa

#endif // CPPA_CONFIG_HPP
