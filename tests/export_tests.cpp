#include "project_io.h"	// Подключает расчет и экспорт книг проекта
#include <filesystem>	// Создает собственный каталог проверочных документов
#include <fstream>	// Записывает независимые ожидаемые значения расчетного ядра
#include <iomanip>	// Сохраняет ожидаемые числа без экспортного округления
#include <iostream>	// Сообщает причину отказа проверочного задания
void fixture(const std::filesystem::path& directory, const char* name, Project p) {	// Создает книгу и независимый снимок исходного результата
    Run run; run.project = p; run.cooling.calculate(p);	// Рассчитывает полный набор пяти участков
    exportWorkbook(directory / (std::string(name) + ".xlsx"), run);	// Проверяет действующий публичный интерфейс выгрузки Excel
    std::ofstream expected(directory / (std::string(name) + ".json")); expected << std::setprecision(17);	// Сохраняет исходную точность ожидаемых чисел
    expected << "{\"volume\":" << totalVolume(p) << ",\"coating\":" << p.coating << ",\"sections\":[";	// Отделяет физический результат от структуры экспортера
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Передает все пять независимых историй
        const auto& h = run.cooling.section(i); if (i) { expected << ','; }	// Разделяет элементы корректного JSON
        expected << "{\"completed\":"; if (h.completedAt) { expected << *h.completedAt; } else { expected << "null"; }	// Различает событие и отсутствие затвердевания
        expected << ",\"samples\":["; bool first = true;	// Начинает массив полных расчетных кадров
        for (const auto& sample : h.samples) {	// Сохраняет каждый исходный кадр до округления
            if (!first) { expected << ','; } first = false; expected << '[' << sample.seconds;	// Сохраняет физическое время текущего кадра
            for (const auto& z : sample.phase.zones) { expected << ',' << z.volumeShare * cylinderVolume(p.dimensions[i]); }	// Получает объемы напрямую из фазового результата
            for (const auto& z : sample.phase.zones) { expected << ',' << z.thickness * 1000; }	// Независимо переводит толщины метров в миллиметры
            expected << ',' << sample.phase.hottestTemperature << ',' << sample.axisTemperature << ',' << sample.surfaceTemperature << ']';	// Сохраняет температуры для проверки числового примера документации
        }	// Завершает текущий блок обработки
        expected << "]}";	// Завершает историю одного участка
    }	// Завершает текущий блок обработки
    expected << "]}";	// Завершает переносимый файл ожидаемых чисел
}	// Завершает текущий блок обработки
int main(int argc, char** argv) {	// Формирует проверочные книги по команде CTest
    try {	// Передает диагностике любое исключение расчетного задания
        if (argc != 2) { throw std::runtime_error("Нужен каталог проверочных книг"); }	// Требует явно выделенный каталог для проверки
        const std::filesystem::path directory(argv[1]); std::filesystem::create_directories(directory);	// Использует собственный каталог внутри сборки
        Project p; fixture(directory, "default", p);	// Проверяет новый исходный проект с автоматическим завершением
        p.autoTime = false; p.hours = 2.03; p.step = 37; p.diameterStep = 20; fixture(directory, "irregular", p);	// Проверяет интерполяцию и неполный последний интервал
        p.hours = 0.07; p.externalTransfer = 0; fixture(directory, "partial", p);	// Проверяет короткую историю без полного затвердевания
        const auto guarded = directory / "guarded.xlsx"; std::ofstream(guarded) << "original";	// Создает собственный файл проверки защищенного сохранения
        bool rejected = false; try { exportWorkbook(guarded, Run{}); } catch (...) { rejected = true; }	// Отклоняет отсутствие расчетной истории
        std::ifstream old(guarded); std::string value; old >> value;	// Читает документ после неудачной попытки выгрузки
        if (!rejected || value != "original") { throw std::runtime_error("Прежний файл поврежден"); }	// Требует сохранения содержимого при отказе экспорта
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }	// Возвращает причину отказа в журнал тестирования
    return 0;	// Подтверждает успешную генерацию всех проверочных случаев
}	// Завершает текущий блок обработки
