/** @file
 *  @brief         Модуль конфигурирования приложения
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#include "../include/configure.h"

void make_configuration(appconfig_t &_ac, int argc, char *argv[])
{
    using namespace boost::program_options;
    options_description desc{"Options"};
    std::string str_command;
    // default server and client settings
    _ac.m_sc.sig = SIGUSR1;
    _ac.m_sc.m_port = 1080;
    _ac.m_sc.m_addr = "127.0.0.1";
    _ac.m_sc.m_threadnumber = 1;
    _ac.m_cc.m_host = _ac.m_sc.m_addr;
    _ac.m_cc.m_port = _ac.m_sc.m_port;
    desc.add_options()
      ("help,h", "Help screen")
      ("start", "Start server")
      ("stop", "Stop server")
      ("clientmode", "Start Client")
      ("port", value<std::uint16_t>(&_ac.m_sc.m_port), "Port");

    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << '\n';
        return;
    }
    if (vm.count("port"))
    {
        LOG << str_sum("Port: ", _ac.m_sc.m_port);
    }
    if (vm.count("clientmode"))
    {
        LOG << "application is working as client";
        _ac.m_clientmode = true;
    }
    else
    {
        _ac.m_clientmode = false;
        _ac.m_sc.m_threadnumber = 2;
    }
    if (vm.count("start"))
    {
        _ac.m_first = 's';
    }
    else if (vm.count("stop"))
    {
        _ac.m_first = 'd';
    }
    else
    {
        _ac.m_first = 0;
    }
}
