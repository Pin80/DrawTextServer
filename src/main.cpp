/** @file
 *  @brief         Главный Модуль приложения
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#include "../include/common.h"
#include "../include/error.h"
#include "../include/configure.h"
#include "../include/control.h"

#if BUILDTYPE == BUILD_DEBUG
DEFINEWATCHPOINT
#endif

#if defined(__GNUC__) || defined(__MINGW32__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
int main(int argc, char *argv[])
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        using std::cin;
        using std::endl;
        ILogger::start_log();
        auto cntrl = std::make_unique<controller_t>(argc, argv);
        cntrl->exec();
        cntrl.reset();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ILogger::stop_log();
    }
    catch (const boost::program_options::error &ex)
    {
      errspace::show_errmsg(ex.what());
      errspace::show_errmsg(FUNCTION);
      return EXIT_FAILURE;
    }
    catch (const std::runtime_error &ex)
    {
      errspace::show_errmsg(ex.what());
      errspace::show_errmsg(FUNCTION);
      return EXIT_FAILURE;
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#if defined(__GNUC__) || defined(__MINGW32__)
#pragma GCC diagnostic pop
#endif
