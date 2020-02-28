#ifndef CLIENT_CFG_H
#define CLIENT_CFG_H

constexpr static auto CBUFFLIMIT = 2*1024*1024;

#  ifndef __GNUC__
#    define DECL_EXPORT     __declspec(dllexport)
#    define DECL_IMPORT     __declspec(dllimport)
#  else
#    define DECL_EXPORT     __attribute__((visibility("default")))
#    define DECL_IMPORT     __attribute__((visibility("default")))
#    define DECL_HIDDEN     __attribute__((visibility("hidden")))
#  endif

#if defined(CLIB_TEST_LIBRARY)
#  define CLIENT_EXPORT DECL_EXPORT
#else
#  define CLIENT_EXPORT DECL_IMPORT
#endif

#define CONN_TIMEOUT  3
#define WRITE_TIMEOUT  45
#define READ_TIMEOUT  25

#endif // CLIENT_CFG_H
