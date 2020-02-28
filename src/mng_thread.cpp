/** @file
 *  @brief         Модуль менеджмента потока
 *  @author unnamed
 *  @date   created 2018
 *  @date   modified 2018
 *  @version 1.0
 */
#include "../include/mng_thread.h"

#if (USESANITR == 1)
    //
#endif
#ifdef ALLOW_FORCED_THREAD_TERMINATION
    #if (CURRENT_PLATFORM == PLATFORM_TYPE_LINUX)
    // supress SANITIZER since it is incomptible with fork()
        #ifdef USESANITR
            #if (USESANITR == 1)
                extern "C" {
                    const char *__asan_default_options() {
                      return "detect_leaks=0";
                    }
                }
            #endif
        #endif
    #endif
#endif

bool threadErrStatus_t::m_destructor_error = false;
