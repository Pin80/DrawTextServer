# DrawText_Sever
Бета версия демо-сервера, которые получает картинку и текст,
накладывает текст на картинку и отправляет назад клиенту.
Клиент загружает картинку из файла, отправляет на сервер и результат сохраняет в файл.

Проект написан на C++17, boost::asio, cairo
Основная платформа - Linux, но можно приспособить и по Windows

Зависимости:
boost (v. 1.65)
cmake (v. 3.5)
GTest
libstdc++fs
librt
cairo

P.S. код сыроват. Все тесты проходит успешно, но могут быть баги.
