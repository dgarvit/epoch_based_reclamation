#ifndef _PORT_
#define _PORT_

#include <stdlib.h>
#include <boost/predef.h>

#if !defined(__SANITIZE_THREAD__) && defined(__has_feature)
    #if __has_feature(thread_sanitizer)
        #define __SANITIZE_THREAD__
    #endif
#endif

#if defined(__SANITIZE_THREAD__)
    #define TSAN_MEMORY_ORDER(tsan_order, _) tsan_order
#else
    #define TSAN_MEMORY_ORDER(_tsan_order, normal_order) normal_order
#endif

#if defined(BOOST_COMP_MSVC_DETECTION)
    #define SELECT_ANY __declspec(selectany)
#elif defined(BOOST_COMP_GNUC_DETECTION)
    #define SELECT_ANY __attribute__((weak))
#else
    #error "Unsupported compiler"
#endif

#endif
