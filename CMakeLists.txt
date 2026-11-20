cmake_minimum_required(VERSION 3.20)																											# Задаёт минимальную версию CMake
project(RollCast LANGUAGES CXX)																													# Объявляет проект на C++
add_executable(RollCast WIN32 src/main.cpp src/mold_view.cpp src/cooling.cpp src/cooling_view.cpp src/phase_change.cpp src/mold_cooling.cpp src/project.cpp src/project_io.cpp)	# Собирает окно и модуль статического разреза
set_target_properties(RollCast PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}>")												# Записывает exe в корень при любой конфигурации
target_compile_features(RollCast PRIVATE cxx_std_20)																							# Включает стандарт C++20 для приложения
target_compile_definitions(RollCast PRIVATE UNICODE)																							# Выбирает Unicode для системных идентификаторов Win32
target_link_libraries(RollCast PRIVATE user32 gdi32 comctl32 comdlg32 gdiplus)																				# Подключает оконные функции кисти и системный ползунок
if(MINGW)																																		# Выбирает параметры компоновки для MinGW-w64
    target_link_options(RollCast PRIVATE -municode -static)																						# Выбирает вход Unicode и встраивает библиотеки компилятора
elseif(MSVC)																																	# Выбирает параметры для компилятора Microsoft
    target_compile_options(RollCast PRIVATE /utf-8)																								# Задаёт кодировку исходников и строковых литералов
endif()																																			# Завершает выбор параметров компилятора
include(CTest)																																	# Подключает средства запуска контрольных тестов
if(BUILD_TESTING)																																# Собирает проверку при включённых тестах
    add_executable(rollcast_cooling_tests tests/cooling_tests.cpp src/cooling.cpp src/phase_change.cpp src/mold_cooling.cpp src/project.cpp src/project_io.cpp)	# Собирает контроль расчётного модуля
    target_include_directories(rollcast_cooling_tests PRIVATE src)																				# Открывает заголовки ядра контрольным примерам
    target_compile_features(rollcast_cooling_tests PRIVATE cxx_std_20)																			# Включает стандарт C++20 для тестов
    add_executable(rollcast_project_tests tests/project_tests.cpp src/cooling.cpp src/phase_change.cpp src/mold_cooling.cpp src/project.cpp src/project_io.cpp)	# Собирает проверки пользовательского проекта
    target_include_directories(rollcast_project_tests PRIVATE src)																							# Открывает интерфейс расчётного ядра
    target_compile_features(rollcast_project_tests PRIVATE cxx_std_20)																						# Включает стандарт контрольных примеров
    add_test(NAME project_workflow COMMAND rollcast_project_tests)																							# Регистрирует проверку файлов параметров и отмены
    add_test(NAME cylinder_cooling COMMAND rollcast_cooling_tests)																				# Регистрирует контроль цилиндра в CTest
endif()																																			# Завершает настройку контрольных примеров
