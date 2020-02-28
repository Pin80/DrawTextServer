# cmake не ниже 3.5
cmake_minimum_required(VERSION 3.5)
# С++
project(drawtext LANGUAGES CXX)

# Требуется поддержка 17го стандарта
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============== Настройка режимов сборки приложения ===============

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

# Санитайзер вкл/выкл для Debug mode
set(USESANITIZER ON)

# Режим сборки приложения

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILDVAL 0)
else()
    set(BUILDVAL 1)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

# =============== Копирование файлов в папку сборки ===============
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Resources DESTINATION ${CMAKE_BINARY_DIR})

# =============== Подключение библиотек и путей хидеров ===============
# Пока поддержка только для Linux
# Подключаем различные библиотеки и хидеры
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
if(UNIX)
    message("Configuring for Linux")
    find_package(GTest REQUIRED)
    find_package(Cairo REQUIRED)
    find_package(Fontconfig REQUIRED)
    message("${Fontconfig_LIBRARIES}")
    include_directories(${GTEST_INCLUDE_DIRS})
    include_directories(${CAIRO_INCLUDE_DIRS})
    include_directories(${FONTCONFIG_INCLUDE_DIRS})
else(UNIX)
    perror("Platform have not supported yet")
endif(UNIX)
if(UNIX)
set(BOOSTLIBPATH "/opt/boost/v165/boost_1_65_0/stage/lib" CACHE PATH "")
set(BOOSTINCPATH "/opt/boost/v165/boost_1_65_0" CACHE PATH "")
set(X86LIBPATH "/usr/lib/x86_64-linux-gnu")
set(FONTCONFIGLIB "${X86LIBPATH}/libfontconfig.so" CACHE FILEPATH "")
include_directories(${BOOSTINCPATH})
#include_directories(${CAIROINCPATH})
set(BOOSTLIBSA
    ${BOOSTLIBPATH}/libboost_iostreams.a
    ${BOOSTLIBPATH}/libboost_system.a
    ${BOOSTLIBPATH}/libboost_date_time.a
    ${BOOSTLIBPATH}/libboost_thread.a
    ${BOOSTLIBPATH}/libboost_regex.a
    ${BOOSTLIBPATH}/libboost_serialization.a
    ${BOOSTLIBPATH}/libboost_program_options.a
    )

#${FONTCONFIG_LIBRARIES} почeмуто приводит к ошибке
set(CAIROLIBSA ${FONTCONFIGLIB} ${CAIRO_LIBRARIES} )

set(LIBLISTAPP
    ${BOOSTLIBSA} -lstdc++fs -lrt ${GTEST_LIBRARIES} ${CAIROLIBSA})

set(LIBLISTAPP2
    ${LIBLISTAPP} ${CMAKE_BINARY_DIR}/lib${PROJECT_NAME}_clib.a)

set(LIBLISTLIB
    ${BOOSTLIBSA} -lrt)

else(UNIX)
    message(FATAL_ERROR "Only linux is supported")
endif(UNIX)

# =============== Установка флагов компиляции ===============
# Компактные структуры, position independent code
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-padded -fsplit-stack -fPIC")
# экспериментальная фича
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} \
    -finput-charset=UTF-8 -fextended-identifiers -fexec-charset=UTF-8 ")

set(SANITDIR 0)
if(UNIX)
    message("Configuring for Linux")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    if (${USESANITIZER})
        message("== sanitizer is enabled ===")
        set(SANITDIR 1) #-Wl,--stack,4194304
        set(ENV{LSAN_OPTIONS} "verbosity=1:log_threads=1")
        set(SANIT_FLAGS "-fno-omit-frame-pointer -fsanitize=address")
        set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANIT_FLAGS}")
        set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} ${SANIT_FLAGS}")
    endif(${USESANITIZER})
    set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -Wl,--heap,41943040")
    set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -Wl,--stack,41943040")
else(UNIX)
    perror("Platform have not supported yet")
endif(UNIX)

# =============== Добавление исходников и хидеров ===============
# Файлы исходников и хидеров для всех целей
set(SRC_LIST
    ./src/mng_thread.cpp
    ./src/error.cpp
    ./src/logger.cpp
    ./src/watchdog.cpp
    ./src/processor.cpp
    ./src/packet.cpp
    ./include/common.h
    ./include/error.h
    ./include/packet.h
    ./include/mng_thread.h
    ./include/logger.h
    ./include/watchdog.h
    ./include/processor.h
    ./include/utility.h
    )

# Файлы исходников и хидеров для всех целей
set(SRC_TEST_SERVER_LIST
    ./include/client.h
    ./include/client_cfg.h
    ./src/server.cpp
    ./include/server.h
    ./include/server_cfg.h
    ./functional/main_test_server.cpp
    ./functional/include/mock_processor1.h
    ./functional/include/mock_server.h
    )

# Файлы исходников и хидеров для тестирование клиента
set(SRC_TEST_CLIENT_LIST
    ./src/client.cpp
    ./include/client.h
    ./include/client_cfg.h
    ./functional/main_test_client.cpp
    ./functional/include/mock_processor2.h
    )

# Файлы исходников и хидеров для клиента в виде библиотеки
set(SRC_CLIB_LIST
    ./src/client.cpp
    ./include/client.h
    ./include/client_cfg.h
    )

# Файлы исходников и хидеров для тестов отдельных компонент
set(SRC_TEST_GEN_LIST
    ./unittests/main_test_general.cpp
    ./include/comb_packet.h
    ./src/comb_packet.cpp
    )

# Файлы исходников и хидеров для основного приложения
set(SRC_MAIN_LIST
    ./include/client.h
    ./include/client_cfg.h
    ./src/server.cpp
    ./src/image_sproc.cpp
    ./src/image_cproc.cpp
    ./src/comb_packet.cpp
    ./src/configure.cpp
    ./src/control.cpp
    ./include/server.h
    ./include/server_cfg.h
    ./include/image_sproc.h
    ./include/image_cproc.h
    ./include/comb_packet.h
    ./include/configure.h
    ./include/control.h
    ./src/main.cpp
    )


file(GLOB doc_files
    "./doc/uml/*"
)

# Различные файлы
set(PROJECT_OTHER_LIST
    ./TODO.txt
    ./KNOWNISSUES.txt
    ./notes.doc
)

# =============== Определение целей ===============
# Фейковая цель для документации
add_custom_target(useful_files SOURCES ${PROJECT_OTHER_LIST} ${doc_files})

# Модульные тесты рабты сервера
add_executable(${PROJECT_NAME}_stest ${SRC_LIST} ${SRC_TEST_SERVER_LIST})
target_link_libraries(${PROJECT_NAME}_stest ${LIBLISTAPP2}  pthread)
target_compile_definitions(${PROJECT_NAME}_stest  PRIVATE
    TARGET="${PROJECT_NAME}" USESANITR=${SANITDIR} USE_DO=1 BUILDTYPE=${BUILDVAL})
add_dependencies(${PROJECT_NAME}_stest ${PROJECT_NAME}_clib)

# Клиент в виде библиотеки
add_library(${PROJECT_NAME}_clib STATIC ${SRC_LIST} ${SRC_CLIB_LIST})
target_link_libraries(${PROJECT_NAME}_clib ${LIBLISTLIB}  pthread)
target_compile_definitions(${PROJECT_NAME}_clib  PRIVATE
    TARGET="${PROJECT_NAME}")
target_compile_definitions(${PROJECT_NAME}_clib  PRIVATE
    CLIB_TEST_LIBRARY=1 USESANITR=${SANITDIR} USE_DO=1 BUILDTYPE=${BUILDVAL})

# Модульныные тесты клиента
add_executable(${PROJECT_NAME}_ctest ${SRC_LIST} ${SRC_TEST_CLIENT_LIST})
target_link_libraries(${PROJECT_NAME}_ctest ${LIBLISTAPP}  pthread)
target_compile_definitions(${PROJECT_NAME}_ctest  PRIVATE
    TARGET="${PROJECT_NAME}" USESANITR=${SANITDIR} USE_DO=1 BUILDTYPE=${BUILDVAL})

# Модульные тесты отдельных компонент приложения
add_executable(${PROJECT_NAME}_gen ${SRC_LIST} ${SRC_TEST_GEN_LIST})
target_link_libraries(${PROJECT_NAME}_gen ${LIBLISTAPP}  pthread)
target_compile_definitions(${PROJECT_NAME}_gen PRIVATE
    TARGET="${PROJECT_NAME}" USESANITR=${SANITDIR} USE_DO=1 BUILDTYPE=${BUILDVAL})

# Главная цель. Можно запускать как в режиме клиента (параметр --clientmode)
# так и в режиме сервера (режим по умолчанию)
add_executable(${PROJECT_NAME} ${SRC_LIST} ${SRC_MAIN_LIST})
target_link_libraries(${PROJECT_NAME} ${LIBLISTAPP2} pthread )
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TARGET="${PROJECT_NAME}" USESANITR=${SANITDIR} USE_DO=1 BUILDTYPE=${BUILDVAL})
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_clib)
