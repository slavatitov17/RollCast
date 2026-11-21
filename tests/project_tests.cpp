#include "project_io.h"	// Подключает новую схему пяти отделений
#include "about_content.h"	// Проверяет структуру встроенного объяснения
#include <chrono>	// Различает временные каталоги разных проверок
#include <cmath>	// Сравнивает геометрию и фазовые балансы
#include <fstream>	// Проверяет записанные проекты и CSV
#include <iostream>	// Сообщает итог контрольных примеров
#include <sstream>	// Независимо разбирает строки CSV
namespace {	// Скрывает вспомогательные функции проверки
void require(bool value, const char* message) {	// Останавливает тест при нарушении инварианта
    if (!value) { throw std::runtime_error(message); }	// Передаёт конкретную причину отказа
}	// Завершает программу проверки новой ветки
struct Temporary {	// Управляет только собственными временными файлами
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("rollcast-five-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));	// Выбирает уникальный каталог текущего теста
    Temporary() { std::filesystem::create_directory(path); }	// Создаёт изолированное место проверки
    ~Temporary() { std::error_code error; std::filesystem::remove_all(path, error); }	// Удаляет только созданный тестом каталог
};	// Завершает управление временными документами
void checkFiles() {	// Проверяет воспроизводимость всех новых входных данных
    Temporary files; Project p; p.autoTime = false;	// Описывает параметры текущего действия
    for (std::size_t i = 0; i < sectionCount; ++i) { p.dimensions[i] = {600 + static_cast<int>(i) * 10, 900 + static_cast<int>(i) * 20}; }	// Различает все пять пар размеров
    p.chill = 350.1; p.coating = 1.2; p.barrelFlask = 31.3;	// Различает стенки окружающие бочку
    p.sand = 146.4; p.flask = 21.5; p.pipeDiameter = 111.6;	// Различает форму шейки и литниковый канал
    p.materials = {{{7100, 760, 31}, {2910, 410, 2.6}, {7810, 510, 46}, {1560, 810, 1.2}}};	// Проверяет двенадцать свойств материалов
    p.phase = {1370, 1460, 260000}; p.metalTemperature = 1530;	// Проверяет фазовые и начальные параметры
    p.formTemperature = 201; p.paintTemperature = 202; p.ironTemperature = 203; p.sandTemperature = 204; p.ironLiquidus = 1240; p.ironSolidus = 1130; p.airTemperature = 24; p.externalTransfer = 4;	// Описывает параметры текущего действия
    p.hours = 0.1; p.step = 27; p.diameterStep = 20;	// Создаёт неполный последний временной шаг
    const auto path = files.path / L"Пять отделений.roll";	// Проверяет русское имя файла проекта
    saveProject(path, p); require(loadProject(path) == p, "New project lost an input");	// Сверяет все поля после открытия
    p.autoTime = true; saveProject(path, p); require(loadProject(path) == p, "Auto file roundtrip failed");	// Сохраняет специальный режим времени без потери настроек
    p.autoTime = false; saveProject(path, p);	// Возвращает короткий фиксированный пример
    const auto legacyPath = files.path / L"Прежний формат.roll";	// Создает собственный файл версии два
    { std::ifstream input(path); std::ofstream output(legacyPath); std::string row; std::getline(input, row); output << "RollCast 2\n"; for (int k = 0; k < 13; ++k) { std::getline(input, row); output << row << '\n'; } }	// Исключает новые настройки из старого формата
    const auto legacy = loadProject(legacyPath);	// Проверяет совместимость пяти отделений прежней версии
    require(!legacy.autoTime && legacy.dimensions == p.dimensions && legacy.ironTemperature == p.formTemperature && legacy.paintTemperature == p.formTemperature && legacy.sandTemperature == p.formTemperature, "Legacy v2 interpretation changed");	// Сохраняет числовой горизонт и прежнюю общую температуру
    Run run; run.project = p; run.cooling.calculate(p);	// Выполняет короткий независимый расчёт
    saveProject(path, p, &run); require(loadProject(path) == p, "Summary changed inputs");	// Проверяет чтение при наличии итоговой сводки
    const auto csv = files.path / L"Полная история.csv"; exportHistory(csv, run);	// Выгружает пять полных временных историй
    std::ifstream stream(csv, std::ios::binary); std::string line;	// Открывает файл независимо от экспортёра
    std::getline(stream, line); require(line.starts_with("\xEF\xBB\xBF"), "CSV UTF-8 marker missing");	// Проверяет кодировку русских названий
    std::size_t rows = 0;	// Считает полные сохранённые отсчёты
    while (std::getline(stream, line)) {	// Проверяет каждую строку выгрузки
        std::istringstream source(line); std::vector<std::string> fields; std::string field;	// Подготавливает независимый разбор колонок
        while (std::getline(source, field, ';')) { fields.push_back(field); }	// Разделяет текстовые и числовые поля
        require(fields.size() == 9, "CSV field count mismatch");	// Требует все девять объявленных столбцов
        const auto section = rows / run.cooling.sampleCount();	// Определяет номер выгружаемого отделения
        const double volume = cylinderVolume(p.dimensions.at(section));	// Находит ожидаемую вместимость цилиндра
        const double zones = std::stod(fields[4]) + std::stod(fields[5]) + std::stod(fields[6]);	// Суммирует три геометрические зоны
        const double phases = std::stod(fields[7]) + std::stod(fields[8]);	// Суммирует две интегральные фазы
        require(std::abs(zones - volume) < 1e-10 && std::abs(phases - volume) < 1e-10, "CSV phase or zone balance failed");	// Проверяет оба разбиения полного объёма
        require(std::stod(fields[1]) == run.cooling.section(section).samples[rows % run.cooling.sampleCount()].seconds, "CSV time mismatch");	// Сверяет каждый момент полной истории
        ++rows;	// Учитывает проверенный температурный кадр
    }	// Завершает проверку полной исходной геометрии
    require(rows == sectionCount * run.cooling.sampleCount(), "One section missing in CSV");	// Требует историю каждого из пяти отделений
    require(run.cooling.section(4).samples.back().seconds == 360, "Final partial time step missing");	// Сверяет точную длительность расчёта
    std::ofstream(path, std::ios::trunc) << "RollCast 1\n"; bool rejected = false;	// Создаёт несовместимую схему прежней ветки
    try { loadProject(path); } catch (const std::exception&) { rejected = true; }	// Проверяет неполные входные данные
    require(rejected, "Four-section format accepted as five");	// Исключает скрытую подмену старого проекта
    std::ofstream(path, std::ios::trunc) << "RollCast 2\n"; rejected = false;	// Создаёт обрезанный документ новой версии
    try { loadProject(path); } catch (const std::exception&) { rejected = true; }	// Проверяет неполные входные данные
    require(rejected, "Truncated project accepted");	// Запрещает загрузку повреждённого проекта
}	// Завершает программу проверки новой ветки
void checkModel() {	// Проверяет влияние реальных полей на модель
    Project p; p.autoTime = false;	// Описывает параметры текущего действия
    require(totalLength(p) == 3300 && outerDiameter(p) == 1040, "Incorrect monolithic dimensions");	// Описывает параметры текущего действия
    require(metalCells(p, 2) == 520, "Diameter step interpreted as radius step");	// Описывает параметры текущего действия
    for (std::size_t i = 0; i < sectionCount; ++i) { p.dimensions[i] = {400, 500 + static_cast<int>(i) * 100}; }	// Сохраняет диаметр при разных осевых размерах
    p.hours = 1; p.diameterStep = 25; p.coating = 20;	// Создаёт быстрый контроль теплообмена
    MoldCooling original; original.calculate(p);	// Рассчитывает исходные слои выбранной конструкции
    require(original.section(0).samples.back().axisTemperature == original.section(4).samples.back().axisTemperature, "Length changed insulated radial temperature");	// Проверяет независимость температуры от осевой длины
    const auto a = phaseVolumes(p.dimensions[0], original.section(0).samples.back());	// Вычисляет фазовые объёмы короткого цилиндра
    const auto b = phaseVolumes(p.dimensions[4], original.section(4).samples.back());	// Вычисляет фазовые объёмы длинного цилиндра
    require(std::abs(b.liquid - a.liquid * 1.8) < 1e-10, "Length did not scale volumes");	// Требует правильное геометрическое масштабирование объёма
    p.materials[1].conductivity = 0.1; MoldCooling insulated; insulated.calculate(p);	// Уменьшает теплопроводность только противопригарного покрытия
    require(insulated.section(0).samples.back().axisTemperature > original.section(0).samples.back().axisTemperature, "Coating conductivity does not affect cooling");	// Проверяет физический знак влияния краски
    require(std::abs(insulated.section(2).relativeEnergyError) < 1e-8, "Multilayer model lost energy");	// Проверяет общий энергетический баланс бочки
    require(insulated.section(2).samples.front().profile.size() == 18, "Compact profile node count mismatch");	// Проверяет центры колец ось и стенку
    require(std::abs(insulated.section(0).samples.back().profile.front() - insulated.section(0).samples.back().axisTemperature) < 0.001, "Compact profile lost accuracy");	// Ограничивает округление визуального температурного профиля
    require(filledSections(p, 0) == std::array<double, sectionCount>{}, "Filling does not start empty");	// Требует пустую форму в начале сценария
    const auto full = filledSections(p, 1);	// Получает завершённое наглядное заполнение
    for (const double fraction : full) { require(std::abs(fraction - 1) < 1e-12, "Incomplete final filling"); }	// Требует полную вместимость каждого отделения
    const auto beginning = filledSections(p, 0.21);	// Выбирает первый момент подъёма уровня
    require(beginning[4] > 0 && beginning[0] == 0, "Filling does not move bottom-up");	// Проверяет движение от нижнего отделения вверх
    std::stop_source stop; stop.request_stop(); bool cancelled = false;	// Создаёт отменённое фоновое задание
    try { original.calculate(p, stop.get_token()); } catch (const std::exception&) { cancelled = true; }	// Проверяет остановку до публикации новой истории
    require(cancelled && original.sampleCount() == 121, "Cancellation damaged previous result");	// Сохраняет целостность уже принятого результата
}	// Завершает программу проверки новой ветки
void checkFullDefaults() {	// Проверяет полный пример на мелкой сетке
    Project p; MoldCooling result; result.calculate(p);	// Рассчитывает все входные значения со снимка
    require(result.completedAt().has_value(), "Auto did not reach full solidification");	// Требует реальное завершение всех пяти отделений
    require(result.section(0).samples.back().seconds >= *result.completedAt(), "Auto ended before solidification");	// Исключает остановку до последнего отделения
    require(result.section(0).samples.back().seconds - *result.completedAt() <= p.step + 1e-6, "Auto overshot by more than one step");	// Ограничивает дискретность автоматической остановки
    for (std::size_t i = 1; i < sectionCount; ++i) { require(result.section(i).samples.size() == result.sampleCount(), "Auto frame alignment failed"); }	// Согласует шкалу всех пяти отделений
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Сверяет каждое из пяти отделений
        const auto& h = result.section(i);	// Получает полную температурную историю отделения
        require(h.relativeEnergyError < 1e-8, "Fine default grid lost energy");	// Проверяет энергию без экранного округления
        require(h.samples.back().phase.hottestTemperature <= p.phase.solidus, "Auto final frame is not solid");	// Проверяет физическое состояние в конце автоматической истории
        require(h.maxIronTemperature >= p.ironTemperature, "Iron heat monitor missing");	// Проверяет включение контроля начальной температуры оснастки
        const auto v = phaseVolumes(p.dimensions[i], h.samples.back());	// Получает итоговые интегральные фазовые объёмы
        require(std::abs(v.solid + v.liquid - cylinderVolume(p.dimensions[i])) < 1e-9, "Fine grid phase balance failed");	// Проверяет сохранение геометрического объёма отделения
        std::cout << "Section " << i + 1 << " energy " << h.relativeEnergyError << " final axis/wall " << h.samples.back().axisTemperature << ' ' << h.samples.back().surfaceTemperature << '\n';	// Показывает независимые показатели полного примера
    }	// Завершает проверку полной исходной геометрии
}	// Завершает программу проверки новой ветки
void checkTimeModes() {	// Проверяет независимое поведение двух режимов времени
    Project p; p.autoTime = false; p.hours = 20; p.step = 120; p.diameterStep = 20;	// Создает быстрый длинный фиксированный расчет
    MoldCooling fixed; fixed.calculate(p);	// Рассчитывает весь заданный числовой горизонт
    require(fixed.completedAt() && *fixed.completedAt() < 20*3600, "Fixture must finish before manual horizon");	// Обеспечивает завершение раньше заданного конца
    require(fixed.section(4).samples.back().seconds == 20*3600, "Manual horizon stopped at solidification");	// Исключает раннюю остановку числового режима
    Project hot = p; hot.hours = 0.01; hot.ironTemperature = 1200; MoldCooling monitored; monitored.calculate(hot);	// Создает нагрев оснастки выше контрольного солидуса
    require(monitored.section(2).maxIronTemperature >= hot.ironSolidus, "Iron solidus monitoring failed");	// Проверяет обнаружение превышения контрольного порога
    p.autoTime = true; p.metalTemperature = p.paintTemperature = p.ironTemperature = p.sandTemperature = 1585; p.externalTransfer = 0;	// Создает изолированную изотермическую систему без охлаждения
    bool rejected = false;	// Отслеживает явное сообщение о недостижимом завершении
    try { fixed.calculate(p); } catch (const std::exception&) { rejected = true; }	// Проверяет ограничение бесконечного автоматического поиска
    require(rejected && fixed.section(4).samples.back().seconds == 20*3600, "Auto failure damaged prior history");	// Сохраняет предыдущую историю при неудачном поиске
}	// Завершает программу проверки новой ветки
void checkExplanation() {	// Проверяет наличие раскрываемой пользователю теории
    const auto theory = aboutContent(true);	// Получает раздел расчётов без интерфейса Windows
    require(!theory.empty() && theory.front().text == L"Ограничения", "Theory must start with limitations");	// Делает допущения первым разделом справки
    int formulas = 0, snippets = 0;	// Считает математические и программные объяснения
    for (const auto& block : theory) { formulas += block.kind == HelpKind::formula; snippets += block.kind == HelpKind::code; }	// Проверяет состав последовательного доклада
    require(formulas >= 15 && snippets >= 7, "Theory or code explanation missing");	// Требует теорию вместе с привязкой к реализации
}	// Завершает программу проверки новой ветки
}	// Завершает программу проверки новой ветки
int main() {	// Запускает контроль новой предметной модели
    try { checkFiles(); checkModel(); checkFullDefaults(); checkTimeModes(); checkExplanation(); }	// Описывает параметры текущего действия
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }	// Сообщает причину отказа в CTest
    std::cout << "Five sections, files, coating, mesh and explanation passed\n"; return 0;	// Подтверждает успешный результат всех проверок
}	// Завершает программу проверки новой ветки
