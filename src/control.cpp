/** @file
 *  @brief         Модуль управления
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#include "../include/control.h"

static std::atomic_bool isRunning = true;

void signal_handler(int _signal)
{
    if (_signal == SIGUSR1)
    {
      std::atomic_signal_fence(std::memory_order_release);
      isRunning.store(false, std::memory_order_relaxed);
    }
}

static std::atomic_bool  is_wdtimeout = false;

void on_wdtimeout(siginfo_t *si) noexcept
{
    is_wdtimeout = true;
    errspace::show_errmsg("watchdog timeout");
}

controller_t::controller_t(int argc, char *argv[])
{
    make_configuration(std::ref(m_ac), argc, argv);
    if (m_ac.m_first == 'q')
    { return; }
    int ic = m_ac.m_first;
    if (m_ac.m_clientmode)
    {
        OUT("DrawText Client is started")
        m_ac.m_cc.m_pproc = std::make_shared<clientImgProcessor_t>(m_ac.m_ci);
        m_ac.m_cc.m_paral = 1;
        m_pclient = std::make_shared<TClient>(m_ac.m_cc);
    }
    else
    {
        OUT("DrawText Server is startet")
        ns_wd::start_wd(1000,10, &on_wdtimeout);
        std::signal(m_ac.m_signal, signal_handler);
        m_ac.m_sc.m_proc = std::make_unique<servimgProcessor_t>(m_ac.m_si);
        m_pserver = TServer::CreateServer(m_ac.m_sc);
    }
}

controller_t::~controller_t()
{
    ns_wd::stop_wd();
    m_pserver.reset();
    TServer::DestroyServer();
    m_pclient.reset();
    m_ac.m_sc.m_proc.reset();
    m_ac.m_cc.m_pproc.reset();
}

void controller_t::exec()
{
    const char * server_help =
 R"(    s - start server
    d - stop server
    h - this help
    i - info
    q - quit )";
    const char * client_help =
 R"(    c - send command
    h - this help
    i - info
    q - quit )";
    if (m_ac.m_first == 'q')
    { return; }
    int ic = m_ac.m_first;
    char action = 0;
    OUT("for help input 'h' and press enter.")
    while(isRunning)
    {
        OUT("enter command:")
        while (isRunning)
        {
            ic = std::cin.peek();
            std::cin.ignore();
            if (((ic > 'a') && (ic < 'z')))
            {
                action = (char)ic;
                ic = 0;
                std::cin.ignore();
                break;
            }
            if ((ic == '\n'))
            {
                action = (char)ic;
                ic = 0;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        switch (action)
        {
            case 'q':
            {
                isRunning = false;
                break;
            }
            case 's':
            {
                if (!m_ac.m_clientmode)
                {
                    if (m_pserver->Start())
                    {
                        OUT("Server is stated")
                    }
                }
                break;
            }
            case 'd':
            {
                if (!m_ac.m_clientmode)
                {
                    if (m_pserver->Stop())
                    {
                        OUT("Server is stopped")
                    }
                }
                break;
            }
            case 'c':
            {
                if (m_ac.m_clientmode)
                {
                    client_connection_action();
                }
                break;
            }
            case 'h':
            {
                if (m_ac.m_clientmode)
                {
                    OUT(client_help)
                }
                else
                {
                    OUT(server_help)
                }
                break;
            }
            case 'i':
            {
                if (m_ac.m_clientmode)
                {
                    OUT(str_sum("Host:", m_ac.m_cc.m_host))
                    OUT(str_sum("Port:", m_ac.m_cc.m_port))
                    OUT(m_pclient->getHandlerStatistics())
                }
                else
                {
                    OUT(str_sum("Host:", m_ac.m_sc.m_addr))
                    OUT(str_sum("Port:", m_ac.m_sc.m_port))
                    OUT(m_pserver->getHandlerStatistics())
                }
                break;
            }
            default:
            {
               OUT("unknown command")
               break;
            }
        }// switch
        action = 0;
    }
}

void controller_t::client_connection_action()
{
    std::uint16_t nattempts = m_ac.m_cc.m_max_reconnattempts;
    bool result_conn = false;
    const auto& ref = m_ac.m_cc.m_pproc.get();
    auto ptr = dynamic_cast<clientImgProcessor_t *>(ref);
    ptr->reset_succcnt();
    do
    {
        result_conn = m_pclient->ConnectAsync();
        if (result_conn)
        {
            bool is_working = m_pclient->isWorking();
            while (is_working)
            {
                is_working = m_pclient->isWorking();
                microdelay(100);
            }
            result_conn = m_pclient->allConnected();
        }
        if (!result_conn)
        {
            OUT("reconnect after:")
            auto delay = m_ac.m_cc.m_reconndelay;
            while(delay)
            {
                OUT(delay)
                millidelay(delay*1000);
                delay--;
            }
        }
        nattempts--;
    } while(!result_conn && nattempts);
    if (result_conn)
    {
        if (ptr->get_succcnt() == m_ac.m_cc.m_paral)
        {
            OUT("RESULT: SUCCESS!")
        }
        else
        {
            OUT("RESULT: FAIL!")
        }
    }
    else
    {
        OUT("RESULT: FAIL!")
    }
}
