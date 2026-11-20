#define NOMINMAX																																			// Отключает макросы минимального и максимального значений
#include <windows.h>																																		// Предоставляет атомарную замену файла в Windows
#include "project_io.h"																																		// Подключает воспроизводимые файловые операции
#include <fstream>																																			// Читает и записывает текстовые файлы
#include <iomanip>																																			// Сохраняет точность чисел и кавычки текста
#include <locale>																																			// Фиксирует десятичную точку в файловом формате
#include <stdexcept>																																		// Передаёт ошибки ввода и вывода интерфейсу
namespace {																																					// Ограничивает видимость файловых помощников
template<class Writer>																																		// Принимает операцию записи содержимого
void atomicWrite(const std::filesystem::path& path, Writer writer) {																						// Заменяет файл только после успешной записи
    auto temporary = path;																																	// Сохраняет каталог выбранного файла
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L".tmp";										// Создаёт уникальное имя промежуточного файла
    try {																																					// Гарантирует удаление незавершённого файла
        std::ofstream stream(temporary, std::ios::binary);																									// Открывает временный файл рядом с целевым
        stream.exceptions(std::ios::badbit | std::ios::failbit);																							// Превращает неполную запись в исключение
        stream.imbue(std::locale::classic());																												// Обеспечивает независимость от языка Windows
        stream << std::setprecision(17);																													// Сохраняет достаточную точность исходных значений
        writer(stream);																																		// Записывает проект или временную историю
        stream.close();																																		// Проверяет завершение записи перед заменой
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {											// Атомарно заменяет целевой файл
            throw std::runtime_error("Не удалось заменить выбранный файл");																					// Сообщает об ошибке файловой системы
        }																																					// Завершает замену файла
    } catch (...) {																																			// Перехватывает неполную файловую операцию
        DeleteFileW(temporary.c_str());																														// Удаляет только свой промежуточный файл
        throw;																																				// Сохраняет исходную причину ошибки
    }																																						// Завершает защиту файловой операции
}																																							// Завершает атомарную запись
}																																							// Завершает область внутренних помощников
Project loadProject(const std::filesystem::path& path) {																									// Читает версионированные входные данные
    std::ifstream stream(path, std::ios::binary);																											// Открывает проект по выбранному пути
    stream.imbue(std::locale::classic());																													// Читает числа с десятичной точкой
    std::string magic;																																		// Принимает идентификатор файлового формата
    int version = 0;																																		// Принимает номер поддерживаемой схемы
    Project p;																																				// Подготавливает отдельный кандидат проекта
    stream >> magic >> version;																																// Читает сигнатуру и номер версии
    if (magic != "RollCast" || version != 1) {																												// Проверяет принадлежность и совместимость файла
        throw std::runtime_error("Неподдерживаемый формат проекта .roll");																					// Объясняет отказ открытия
    }																																						// Завершает проверку версии
    stream >> std::quoted(p.name) >> std::quoted(p.source);																									// Восстанавливает название и происхождение данных
    for (auto& dimension : p.dimensions) {																													// Восстанавливает геометрию четырёх отделений
        stream >> dimension.diameter >> dimension.height;																									// Читает диаметр и осевую длину
    }																																						// Завершает чтение размеров
    for (auto& value : p.sand) { stream >> value; }																											// Читает толщины песчаных слоёв
    for (auto& value : p.flask) { stream >> value; }																										// Читает толщины металлических опок
    for (auto& material : p.materials) {																													// Читает свойства трёх материалов
        stream >> material.density >> material.heatCapacity >> material.conductivity;																		// Восстанавливает плотность теплоёмкость и проводимость
    }																																						// Завершает чтение свойств материалов
    stream >> p.solidDensity >> p.phase.solidus >> p.phase.liquidus >> p.phase.latentHeat;																	// Читает параметры фазового превращения
    stream >> p.metalTemperature >> p.formTemperature >> p.airTemperature;																					// Читает температурные начальные условия
    stream >> p.metalContact >> p.formContact >> p.externalTransfer;																						// Восстанавливает коэффициенты теплообмена
    stream >> p.hours >> p.step >> p.cells >> p.fillingSeconds;																								// Читает настройки времени и сетки
    int circulation = -1;																																	// Принимает строгий признак модели движения
    stream >> circulation;																																	// Читает выбранный режим циркуляции
    if (!stream || (circulation != 0 && circulation != 1)) {																								// Обнаруживает обрыв или повреждение входных данных
        throw std::runtime_error("Проект содержит неполные или повреждённые данные");																		// Сообщает причину отказа
    }																																						// Завершает проверку структуры входов
    p.circulation = circulation == 1;																														// Применяет подтверждённый режим движения
    validateProject(p);																																		// Проверяет физические диапазоны и согласованность
    return p;																																				// Возвращает только полностью проверенный проект
}																																							// Завершает открытие проекта
void saveProject(const std::filesystem::path& path, const Project& p, const Run* run) {																		// Сохраняет входные данные и итоговую сводку
    validateProject(p);																																		// Отклоняет непригодные для воспроизведения исходные данные
    atomicWrite(path, [&](std::ostream& stream) {																											// Подготавливает файл без повреждения предыдущей версии
        stream << "RollCast 1\n" << std::quoted(p.name) << '\n' << std::quoted(p.source) << '\n';															// Записывает версию и текстовые поля
        for (const auto& dimension : p.dimensions) {																										// Сохраняет размеры в порядке модели
            stream << dimension.diameter << ' ' << dimension.height << '\n';																				// Записывает диаметр и длину отделения
        }																																					// Завершает сохранение размеров
        for (const auto value : p.sand) { stream << value << ' '; }																							// Сохраняет толщины песка
        stream << '\n';																																		// Разделяет наборы толщин
        for (const auto value : p.flask) { stream << value << ' '; }																						// Сохраняет толщины опок
        stream << '\n';																																		// Завершает геометрические параметры формы
        for (const auto& material : p.materials) {																											// Сохраняет свойства всех материалов
            stream << material.density << ' ' << material.heatCapacity << ' ' << material.conductivity << '\n';												// Записывает три постоянных свойства
        }																																					// Завершает сохранение материалов
        stream << p.solidDensity << ' ' << p.phase.solidus << ' ' << p.phase.liquidus << ' ' << p.phase.latentHeat << '\n';									// Записывает свойства фазового перехода
        stream << p.metalTemperature << ' ' << p.formTemperature << ' ' << p.airTemperature << '\n';														// Записывает исходные температуры
        stream << p.metalContact << ' ' << p.formContact << ' ' << p.externalTransfer << '\n';																// Записывает коэффициенты теплообмена
        stream << p.hours << ' ' << p.step << ' ' << p.cells << ' ' << p.fillingSeconds << '\n';															// Записывает временные и сеточные настройки
        stream << static_cast<int>(p.circulation) << '\n';																									// Сохраняет выбранный механизм переноса
        if (run && run->project == p) {																														// Добавляет сводку только подходящего результата
            stream << "SUMMARY\n";																															// Отмечает начало необязательных итоговых данных
            for (std::size_t i = 0; i < 4; ++i) {																											// Сохраняет итог каждого отделения
                const auto& history = run->cooling.section(i);																								// Выбирает температурную историю
                const auto volumes = phaseVolumes(p.dimensions[i], history.samples.back());																	// Получает конечные объёмы фаз
                stream << i << ' ' << volumes.solid << ' ' << volumes.liquid << ' ' << history.completedAt.value_or(-1) << '\n';							// Записывает объёмы и доступное время затвердевания
            }																																				// Завершает итоговую сводку
        }																																					// Завершает проверку соответствия результата
    });																																						// Завершает атомарное сохранение проекта
}																																							// Завершает сохранение файла .roll
void exportHistory(const std::filesystem::path& path, const Run& run) {																						// Выгружает полную историю после заполнения формы
    atomicWrite(path, [&](std::ostream& stream) {																											// Записывает CSV через защищённую замену
        stream << "\xEF\xBB\xBF";																															// Помечает кодировку русских заголовков для Excel
        stream << "Отделение;Время с;Ось °C;Стенка °C;Твёрдая зона м³;Двухфазная зона м³;Жидкая зона м³;Твёрдая фаза м³;Жидкая фаза м³;Масштаб скорости м/с;Оценка твёрдого объёма м³;Оценка дефицита м³\n";	// Задаёт единицы и смысл столбцов
        constexpr const char* names[]{"Нижняя шейка", "Бочка", "Верхняя шейка", "Прибыль"};																	// Называет четыре отделения в кодировке UTF-8
        for (std::size_t section = 0; section < 4; ++section) {																								// Выгружает каждое отделение полностью
            const double volume = cylinderVolume(run.project.dimensions[section]);																			// Получает фиксированный объём отделения
            for (const auto& sample : run.cooling.section(section).samples) {																				// Сохраняет каждый рассчитанный момент
                const auto phases = phaseVolumes(run.project.dimensions[section], sample);																	// Учитывает фазы внутри двухфазной зоны
                stream << names[section] << ';' << sample.seconds << ';' << sample.axisTemperature << ';' << sample.surfaceTemperature;						// Записывает время и температуры
                for (const auto& zone : sample.phase.zones) { stream << ';' << zone.volumeShare * volume; }													// Записывает геометрические объёмы трёх зон
                stream << ';' << phases.solid << ';' << phases.liquid << ';' << sample.circulationSpeed;													// Записывает интегральные фазовые объёмы и скорость
                if (run.project.solidDensity > 0) {																											// Включает дополнительную оценку изменения плотности
                    const double solid = phases.solid * run.project.materials[0].density / run.project.solidDensity;										// Оценивает объём затвердевшей массы
                    stream << ';' << solid << ';' << volume - solid - phases.liquid;																		// Оценивает дефицит без моделирования питания
                } else {																																	// Обрабатывает отключённую оценку усадки
                    stream << ";;";																															// Оставляет необязательные столбцы пустыми
                }																																			// Завершает дополнительные оценки
                stream << '\n';																																// Завершает одну строку полной истории
            }																																				// Завершает выгрузку одного отделения
        }																																					// Завершает выгрузку всех отделений
    });																																						// Завершает защищённое сохранение CSV
}																																							// Завершает экспорт временных историй
