cmake_minimum_required(VERSION 3.20) # Задаёт минимальную версию CMake.
project(RollCast LANGUAGES CXX) # Объявляет проект на C++.
add_executable(RollCast WIN32 src/main.cpp) # Создаёт приложение Windows без консольного окна.
target_compile_features(RollCast PRIVATE cxx_std_20) # Включает стандарт C++20 для приложения.
target_compile_definitions(RollCast PRIVATE UNICODE) # Выбирает Unicode для системных идентификаторов Win32.
target_link_libraries(RollCast PRIVATE user32 gdi32) # Подключает оконные функции Win32 и кисти GDI.
if(MINGW) # Выбирает параметры компоновки для MinGW-w64.
    # Выбирает точку входа Unicode и встраивает библиотеки GCC.
    target_link_options(RollCast PRIVATE -municode -static-libgcc -static-libstdc++)
elseif(MSVC) # Выбирает параметры для компилятора Microsoft.
    target_compile_options(RollCast PRIVATE /utf-8) # Задаёт кодировку исходников и строковых литералов.
endif() # Завершает выбор параметров компилятора.
