if(POLICY CMP0135)	# Использует время распаковки проверенных архивов
    cmake_policy(SET CMP0135 NEW)	# Предотвращает устаревание зависимостей при повторной сборке
endif()	# Завершает настройку совместимой политики CMake
include(FetchContent)	# Подключает загрузку закрепленных исходников библиотек
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)	# Разрешает сборку zlib современным CMake
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)	# Встраивает библиотеки в исполняемый файл
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)	# Исключает сторонние демонстрационные приложения
FetchContent_Declare(rollcast_zlib URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip URL_HASH SHA256=50b24b47bf19e1f35d2a21ff36d2a366638cdf958219a66f30ce0861201760e6)	# Проверяет архив фиксированной версии zlib
FetchContent_MakeAvailable(rollcast_zlib)	# Собирает библиотеку сжатия внутри каталога сборки
set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE)	# Не собирает ненужную динамическую версию библиотеки
set(ZLIB_INCLUDE_DIR "${rollcast_zlib_SOURCE_DIR}" CACHE PATH "" FORCE)	# Передает заголовки поставляемой библиотеки сжатия
set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)	# Выбирает статическую библиотеку вместо системной
if(NOT TARGET ZLIB::ZLIB)	# Предоставляет стандартное имя зависимости сжатия
    add_library(ZLIB::ZLIB ALIAS zlibstatic)	# Связывает экспорт Excel со статическим zlib
endif()	# Завершает регистрацию зависимости сжатия
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)	# Исключает тесты сторонней библиотеки Excel
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)	# Исключает демонстрации сторонней библиотеки Excel
FetchContent_Declare(rollcast_xlsxwriter URL https://github.com/jmcnamara/libxlsxwriter/archive/refs/tags/v1.2.4.zip URL_HASH SHA256=3409bacd1eafc75c6c930756abed228d5f82fc8669f0ac1c35f827c2a114b7cd)	# Проверяет архив фиксированной версии libxlsxwriter
FetchContent_MakeAvailable(rollcast_xlsxwriter)	# Собирает запись книг Excel без установленного Office
