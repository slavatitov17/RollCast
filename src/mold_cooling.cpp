#include "mold_cooling.h"																																			// Подключает общий расчёт отделений формы
#include <algorithm>																																				// Предоставляет поиск самого позднего затвердевания
#include <numbers>																																					// Предоставляет число пи для объёма цилиндра
#include <stdexcept>																																				// Сообщает об ошибочных размерах отделения
#include <utility>																																					// Перемещает временную историю без копирования кадров
double cylinderVolume(const SectionDimensions& dimensions) {																										// Переводит геометрию в полный объём металла
    const double radius = dimensions.diameter / 2000.0;																												// Переводит диаметр в радиус в метрах
    const double length = dimensions.height / 1000.0;																												// Переводит осевую длину в метры
    return std::numbers::pi * radius * radius * length;																												// Умножает площадь основания на длину
}																																									// Завершает расчёт полного объёма
PhaseVolumes phaseVolumes(const SectionDimensions& dimensions, const CoolingSample& sample) {																		// Оценивает объёмы фаз выбранного отделения
    const double volume = cylinderVolume(dimensions);																												// Получает полный объём цилиндра
    const double liquid = volume * sample.phase.liquidVolumeShare;																									// Учитывает интегральную долю жидкости по сечению
    return {volume - liquid, liquid};																																// Возвращает твёрдую и жидкую составляющие
}																																									// Завершает расчёт объёмов отдельных фаз
bool MoldCooling::refresh(const MoldDimensions& dimensions, bool circulation) {																						// Готовит согласованное обновление расчёта формы
    std::array<std::optional<CoolingResult>, demoDimensions.size()> pending;																						// Хранит новые истории до успешного расчёта
    bool changed = false;																																			// Отмечает необходимость обновить шкалу времени
    for (std::size_t i = 0; i < dimensions.size(); ++i) {																											// Сравнивает размеры всех пяти отделений
        const int diameter = dimensions[i].diameter;																												// Читает диаметр этого расчётного цилиндра
        if (diameter <= 0 || dimensions[i].height <= 0) {																											// Проверяет положительность размеров каждого отделения
            throw std::invalid_argument("Invalid section dimensions");																								// Отклоняет некорректную геометрию формы
        }																																							// Завершает проверку размеров отделения
        if (configured || diameters[i] != diameter || hasCirculation != circulation || (circulation && lengths[i] != dimensions[i].height)) {				// Проверяет изменение радиального масштаба
            pending[i] = demonstrationCooling(diameter, demoPhase, circulation ? std::optional<MeltCirculation>{{dimensions[i].height / 1000.0}} : std::nullopt);	// Рассчитывает затвердевание одного отделения
            changed = true;																																			// Отмечает появление новой истории
        }																																							// Завершает решение о пересчёте отделения
    }																																								// Завершает подготовку новых историй
    for (std::size_t i = 0; i < pending.size(); ++i) {																												// Применяет набор успешно завершённых расчётов
        if (pending[i]) {																																			// Проверяет наличие новой истории этого отделения
            histories[i] = std::move(*pending[i]);																													// Заменяет только пересчитанное отделение
            lengths[i] = dimensions[i].height;																														// Запоминает осевой размер нового расчёта
            diameters[i] = dimensions[i].diameter;																													// Запоминает диаметр сохранённого расчёта
        }																																							// Завершает замену истории отделения
    }																																								// Завершает обновление согласованного набора
    configured = false;																																		// Отмечает восстановление демонстрационной постановки
    hasCirculation = circulation;																																	// Запоминает режим согласованного набора
    return changed;																																					// Сообщает изменился ли расчёт температур
}																																									// Завершает обновление охлаждения формы
std::size_t MoldCooling::sampleCount() const {																														// Задаёт размер общей шкалы времени
    return histories.front().samples.size();																														// Читает число кадров первой истории
}																																									// Завершает чтение длины расчёта
const CoolingResult& MoldCooling::section(std::size_t index) const {																								// Открывает температуры одного отделения
    return histories.at(index);																																		// Возвращает историю отделения по его номеру
}																																									// Завершает выбор расчётной истории
PhaseVolumes MoldCooling::totalVolumes(const MoldDimensions& dimensions, std::size_t frame) const {																	// Суммирует объёмы по отделениям формы
    PhaseVolumes total{};																																			// Начинает суммирование с пустого объёма
    for (std::size_t i = 0; i < histories.size(); ++i) {																											// Обходит пять отделений без литниковых каналов
        const auto volumes = phaseVolumes(dimensions[i], histories[i].samples.at(frame));																			// Берёт фазовые объёмы общего момента
        total.solid += volumes.solid;																																// Прибавляет твёрдый металл текущего отделения
        total.liquid += volumes.liquid;																																// Прибавляет жидкий металл текущего отделения
    }																																								// Завершает суммирование отделений формы
    return total;																																					// Возвращает общий объём обеих фаз
}																																									// Завершает расчёт суммарных фазовых объёмов
std::optional<double> MoldCooling::completedAt() const {																											// Определяет время затвердевания всей формы
    double latest = 0;																																				// Сохраняет самый поздний момент затвердевания
    for (const auto& history : histories) {																															// Проверяет достижение солидуса в каждом отделении
        if (!history.completedAt) {																																	// Проверяет наличие ещё жидкого отделения
            return std::nullopt;																																	// Оставляет время за горизонтом расчёта
        }																																							// Завершает проверку окончания затвердевания отделения
        latest = std::max(latest, *history.completedAt);																											// Учитывает наиболее позднее затвердевание
    }																																								// Завершает сравнение времён всех отделений
    return latest;																																					// Возвращает окончание затвердевания последнего отделения
}																																									// Завершает расчёт времени затвердевания формы
void MoldCooling::calculate(const Project& p, std::stop_token stop) {	// Рассчитывает пять отделений монолитного валка
    validateProject(p);	// Проверяет входы до изменения сохранённой истории
    MoldCooling pending;	// Собирает новый согласованный результат отдельно
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Обходит отделения от прибыли вниз
        const auto& metal = p.materials[0];	// Выбирает единственный сплав всего валка
        const auto& paint = p.materials[1];	// Выбирает теплоизоляционную краску
        const auto& iron = p.materials[2];	// Выбирает материал кокиля и опок
        const auto& form = p.materials[i == 2 ? 2 : 3];	// Окружает бочку кокилем остальные участки песком
        const double radius = p.dimensions[i].diameter / 2000.0;	// Вычисляет радиус металлического отделения
        std::vector<CoolingLayer> layers{{radius, metal.conductivity, metal.density, metal.heatCapacity, p.metalTemperature, metalCells(p, i)}};	// Создаёт металлические кольца заданной сетки
        if (p.coating > 0) {	// Добавляет физически присутствующее покрытие
            layers.push_back({p.coating / 1000, paint.conductivity, paint.density, paint.heatCapacity, p.formTemperature, 2});	// Представляет краску двумя конечными объёмами
        }	// Завершает проверку наличия краски
        layers.push_back({(i == 2 ? p.chill : p.sand) / 1000, form.conductivity, form.density, form.heatCapacity, p.formTemperature, 32});	// Добавляет основной охлаждающий слой формы
        layers.push_back({(i == 2 ? p.barrelFlask : p.flask) / 1000, iron.conductivity, iron.density, iron.heatCapacity, p.formTemperature, 8});	// Добавляет наружную металлическую опоку
        const CoolingBoundary boundary{p.airTemperature, p.externalTransfer, p.hours * 3600, p.step, 0, 0, true};	// Задаёт наружную теплоотдачу и идеальные контакты
        pending.histories[i] = solveCooling(layers, boundary, p.phase, std::nullopt, stop);	// Решает теплопроводность без движения расплава
    }	// Завершает пять независимых радиальных расчётов
    pending.configured = true;	// Отмечает пользовательскую постановку
    *this = std::move(pending);	// Публикует только полностью рассчитанный набор
}	// Завершает расчёт монолитного валка
