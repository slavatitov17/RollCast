#include "project_io.h"																																		// Проверяет файлы параметры и согласованность расчёта
#include <cmath>																																			// Сравнивает результаты с числовым допуском
#include <fstream>																																			// Читает экспортированные файлы для независимой проверки
#include <iostream>																																			// Сообщает результат контрольных примеров
#include <sstream>																																			// Разбирает столбцы экспортированной истории
#include <chrono>																																			// Создаёт уникальный каталог текущего теста
#include <limits>																																			// Задаёт недопустимые контрольные значения
namespace {																																					// Ограничивает вспомогательные проверки текущим файлом
void require(bool value, const char* message) {																												// Останавливает тест при нарушении условия
    if (!value) { throw std::runtime_error(message); }																										// Передаёт диагностическое сообщение в CTest
}																																							// Завершает проверку условия
struct Temporary {																																			// Управляет собственными временными файлами теста
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("rollcast-project-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));	// Выбирает уникальный временный каталог
    Temporary() { std::filesystem::create_directory(path); }																								// Создаёт изолированное место проверки
    ~Temporary() { std::error_code error; std::filesystem::remove_all(path, error); }																		// Убирает только созданный текущим тестом каталог
};																																							// Завершает управление временными файлами
void checkProject() {																																		// Проверяет сохранение всех пользовательских исходных данных
    Temporary files; Project project;																														// Создаёт независимый проект и временный каталог
    project.name = "Валок «Проверка»"; project.source = "Источник: измерения";																				// Проверяет русский текст и специальные символы
    for (std::size_t i = 0; i < 4; ++i) {																													// Различает все четыре отделения
        project.dimensions[i] = {600 + static_cast<int>(i), 900 + static_cast<int>(i)};																		// Задаёт различные размеры отделений
        project.sand[i] = 90.1 + i; project.flask[i] = 20.3 + i;																							// Задаёт различные толщины обоих слоёв
    }																																						// Завершает настройку геометрии
    project.materials = {{{7000, 750, 30}, {1600, 1050, 0.9}, {7850, 500, 45}}};																			// Проверяет сохранение всех свойств материалов
    project.phase = {1370, 1460, 260000}; project.solidDensity = 7450;																						// Задаёт новые фазовые параметры
    project.metalTemperature = 1530; project.formTemperature = 25; project.airTemperature = 24;																// Различает три температуры
    project.metalContact = 350; project.formContact = 300; project.externalTransfer = 18;																	// Задаёт конечное контактное сопротивление
    project.hours = 0.5; project.step = 27; project.cells = 24;																								// Проверяет неполный последний временной интервал
    project.fillingSeconds = 74.1234567890123; project.circulation = true;																					// Проверяет точность времени и режим движения
    const auto path = files.path / L"Русский проект.roll";																									// Проверяет путь с русскими буквами
    saveProject(path, project); require(loadProject(path) == project, "Project roundtrip lost inputs");														// Сверяет каждое сохранённое поле
    Run run; run.project = project; run.cooling.calculate(project);																							// Рассчитывает сохранённый пользовательский проект
    saveProject(path, project, &run); require(loadProject(path) == project, "Summary changed project inputs");												// Проверяет совместимость итоговой сводки
    const auto csv = files.path / L"История.csv"; exportHistory(csv, run);																					// Выгружает полную историю четырёх отделений
    std::ifstream input(csv, std::ios::binary); std::string line;																							// Открывает готовую таблицу независимо от экспортёра
    std::getline(input, line); require(line.starts_with("\xEF\xBB\xBF"), "Missing CSV UTF-8 marker");														// Проверяет кодировку русских заголовков
    std::size_t rows = 0;																																	// Считает строки полной истории
    while (std::getline(input, line)) {																														// Проверяет каждую экспортированную строку
        std::vector<std::string> columns; std::istringstream values(line); std::string cell;																// Подготавливает независимый разбор столбцов
        while (std::getline(values, cell, ';')) { columns.push_back(cell); }																				// Разделяет числовые и текстовые значения
        require(columns.size() == 12, "Invalid CSV column count");																							// Требует все объявленные столбцы
        const std::size_t section = rows / run.cooling.sampleCount();																						// Определяет отделение по порядку строк
        const double volume = cylinderVolume(project.dimensions.at(section));																				// Находит ожидаемый геометрический объём
        const double zones = std::stod(columns[4]) + std::stod(columns[5]) + std::stod(columns[6]);															// Суммирует три непересекающиеся зоны
        const double solid = std::stod(columns[7]); const double liquid = std::stod(columns[8]);															// Читает интегральные фазовые объёмы
        const double adjusted = std::stod(columns[10]); const double deficit = std::stod(columns[11]);														// Читает дополнительную оценку плотности
        require(std::abs(zones - volume) < 1e-10 && std::abs(solid + liquid - volume) < 1e-10, "CSV volume balance failed");								// Проверяет оба независимых разбиения объёма
        require(std::abs(adjusted * project.solidDensity - solid * project.materials[0].density) < 1e-8, "Density estimate lost mass");						// Проверяет сохранение затвердевшей массы
        require(std::abs(adjusted + liquid + deficit - volume) < 1e-10, "CSV deficit balance failed");														// Проверяет дополнительный объёмный дефицит
        require(std::stod(columns[1]) == run.cooling.section(section).samples[rows % run.cooling.sampleCount()].seconds, "CSV time mismatch");				// Проверяет время после окончания заполнения
        ++rows;																																				// Учитывает проверенный отсчёт
    }																																						// Завершает проверку экспортированной истории
    require(rows == 4 * run.cooling.sampleCount(), "CSV lost history rows");																				// Требует полную историю каждого отделения
    std::ofstream(path, std::ios::trunc) << "RollCast 1\n\"Broken\"\n";																						// Создаёт намеренно обрезанный собственный файл
    bool rejected = false; try { loadProject(path); } catch (const std::exception&) { rejected = true; }													// Проверяет повреждённый документ
    require(rejected, "Truncated project accepted");																										// Запрещает принятие неполных исходных данных
}																																							// Завершает контроль файлового проекта
void checkPhysics() {																																		// Проверяет влияние новых параметров на расчёт
    Project project; project.hours = 1; project.cells = 24;																									// Создаёт короткий контрольный расчёт
    MoldCooling ideal; ideal.calculate(project);																											// Рассчитывает идеальный межслойный контакт
    project.metalContact = 1; project.formContact = 1;																										// Создаёт значительное контактное сопротивление
    MoldCooling resistant; resistant.calculate(project);																									// Рассчитывает затруднённый теплообмен
    require(resistant.section(1).samples.back().axisTemperature > ideal.section(1).samples.back().axisTemperature, "Contact resistance did not slow cooling");	// Проверяет физический знак влияния контакта
    require(std::abs(resistant.section(1).relativeEnergyError) < 1e-8, "Contact interface lost energy");													// Проверяет единый поток через границу материалов
    require(ideal.section(0).samples.front().profile.size() == 26, "Radial profile missing");																// Требует ячейки ось и стенку
    require(filledSections(project, 0) == std::array<double, 4>{0, 0, 0, 0}, "Filling did not start empty");												// Требует пустую отливку до заливки
    double total = 0; for (const auto& d : project.dimensions) { total += cylinderVolume(d); }																// Исключает литники из объёма отливки
    for (int step = 20; step <= 100; ++step) {																												// Проверяет постоянный объёмный расход
        const double progress = step / 100.0; const auto filled = filledSections(project, progress);														// Выбирает промежуточный момент заполнения
        double actual = 0;																																	// Суммирует поступивший металл
        for (std::size_t i = 0; i < 4; ++i) { actual += cylinderVolume(project.dimensions[i]) * filled[i]; }												// Учитывает вместимость каждого цилиндра
        require(std::abs(actual - total * (progress - 0.2) / 0.8) < 1e-12, "Filling volume rate mismatch");													// Проверяет постоянный объёмный расход
    }																																						// Завершает проверку закона заполнения
    std::stop_source cancellation; cancellation.request_stop(); bool cancelled = false;																		// Готовит отменённое задание
    try { ideal.calculate(project, cancellation.get_token()); } catch (const std::exception&) { cancelled = true; }											// Проверяет прерывание фонового расчёта
    require(cancelled && ideal.sampleCount() == 121, "Cancellation damaged previous history");																// Требует сохранность уже принятого результата
    project.metalContact = std::numeric_limits<double>::quiet_NaN(); bool rejected = false;																	// Задаёт недопустимое контактное значение
    try { validateProject(project); } catch (const std::exception&) { rejected = true; }																	// Проверяет конечность нового исходного параметра
    require(rejected, "Invalid contact accepted");																											// Запрещает неопределённые коэффициенты
}																																							// Завершает проверку новых расчётных параметров
}																																							// Завершает внутренние контрольные функции
int main() {																																				// Запускает контроль пользовательского проекта
    try { checkProject(); checkPhysics(); }																													// Проверяет файлы и расчётные связи
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }																	// Сообщает причину неудачной проверки
    std::cout << "Project, CSV, contacts, filling and cancellation passed\n"; return 0;																		// Подтверждает успешное завершение контроля
}																																							// Завершает программу проверки
