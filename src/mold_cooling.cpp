#include "mold_cooling.h"	// Подключает общий расчет отделений формы
#include <algorithm>	// Предоставляет поиск самого позднего затвердевания
#include <numbers>	// Предоставляет число пи для объема цилиндра
#include <stdexcept>	// Сообщает об ошибочных размерах отделения
#include <utility>	// Перемещает временную историю без копирования кадров
#include <cmath>	// Округляет безопасный горизонт расчета
double cylinderVolume(const SectionDimensions& dimensions) {	// Переводит геометрию в полный объем металла
    const double radius = dimensions.diameter / 2000.0;	// Переводит диаметр в радиус в метрах
    const double length = dimensions.height / 1000.0;	// Переводит осевую длину в метры
    return std::numbers::pi * radius * radius * length;	// Умножает площадь основания на длину
}	// Завершает текущий блок обработки
PhaseVolumes phaseVolumes(const SectionDimensions& dimensions, const CoolingSample& sample) {	// Оценивает объемы фаз выбранного отделения
    const double volume = cylinderVolume(dimensions);	// Получает полный объем цилиндра
    const double liquid = volume * sample.phase.liquidVolumeShare;	// Учитывает интегральную долю жидкости по сечению
    return {volume - liquid, liquid};	// Возвращает твердую и жидкую составляющие
}	// Завершает текущий блок обработки
bool MoldCooling::refresh(const MoldDimensions& dimensions, bool circulation) {	// Готовит согласованное обновление расчета формы
    std::array<std::optional<CoolingResult>, demoDimensions.size()> pending;	// Хранит новые истории до успешного расчета
    bool changed = false;	// Отмечает необходимость обновить шкалу времени
    for (std::size_t i = 0; i < dimensions.size(); ++i) {	// Сравнивает размеры всех пяти отделений
        const int diameter = dimensions[i].diameter;	// Читает диаметр этого расчетного цилиндра
        if (diameter <= 0 || dimensions[i].height <= 0) {	// Проверяет положительность размеров каждого отделения
            throw std::invalid_argument("Invalid section dimensions");	// Отклоняет некорректную геометрию формы
        }	// Завершает текущий блок обработки
        if (configured || diameters[i] != diameter || hasCirculation != circulation || (circulation && lengths[i] != dimensions[i].height)) {	// Проверяет изменение радиального масштаба
            pending[i] = demonstrationCooling(diameter, demoPhase, circulation ? std::optional<MeltCirculation>{{dimensions[i].height / 1000.0}} : std::nullopt);	// Рассчитывает затвердевание одного отделения
            changed = true;	// Отмечает появление новой истории
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    for (std::size_t i = 0; i < pending.size(); ++i) {	// Применяет набор успешно завершенных расчетов
        if (pending[i]) {	// Проверяет наличие новой истории этого отделения
            histories[i] = std::move(*pending[i]);	// Заменяет только пересчитанное отделение
            lengths[i] = dimensions[i].height;	// Запоминает осевой размер нового расчета
            diameters[i] = dimensions[i].diameter;	// Запоминает диаметр сохраненного расчета
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    configured = false;	// Отмечает восстановление демонстрационной постановки
    hasCirculation = circulation;	// Запоминает режим согласованного набора
    return changed;	// Сообщает изменился ли расчет температур
}	// Завершает текущий блок обработки
std::size_t MoldCooling::sampleCount() const {	// Задает размер общей шкалы времени
    return histories.front().samples.size();	// Читает число кадров первой истории
}	// Завершает текущий блок обработки
const CoolingResult& MoldCooling::section(std::size_t index) const {	// Открывает температуры одного отделения
    return histories.at(index);	// Возвращает историю отделения по его номеру
}	// Завершает текущий блок обработки
PhaseVolumes MoldCooling::totalVolumes(const MoldDimensions& dimensions, std::size_t frame) const {	// Суммирует объемы по отделениям формы
    PhaseVolumes total{};	// Начинает суммирование с пустого объема
    for (std::size_t i = 0; i < histories.size(); ++i) {	// Обходит пять отделений без литниковых каналов
        const auto volumes = phaseVolumes(dimensions[i], histories[i].samples.at(frame));	// Берет фазовые объемы общего момента
        total.solid += volumes.solid;	// Прибавляет твердый металл текущего отделения
        total.liquid += volumes.liquid;	// Прибавляет жидкий металл текущего отделения
    }	// Завершает текущий блок обработки
    return total;	// Возвращает общий объем обеих фаз
}	// Завершает текущий блок обработки
std::optional<double> MoldCooling::completedAt() const {	// Определяет время затвердевания всей формы
    double latest = 0;	// Сохраняет самый поздний момент затвердевания
    for (const auto& history : histories) {	// Проверяет достижение солидуса в каждом отделении
        if (!history.completedAt) {	// Проверяет наличие еще жидкого отделения
            return std::nullopt;	// Оставляет время за горизонтом расчета
        }	// Завершает текущий блок обработки
        latest = std::max(latest, *history.completedAt);	// Учитывает наиболее позднее затвердевание
    }	// Завершает текущий блок обработки
    return latest;	// Возвращает окончание затвердевания последнего отделения
}	// Завершает текущий блок обработки
void MoldCooling::calculate(const Project& p, std::stop_token stop) {	// Рассчитывает пять отделений монолитного валка
    validateProject(p);	// Проверяет входы до изменения сохраненной истории
    MoldCooling pending;	// Собирает новый согласованный результат отдельно
    double duration = p.hours * 3600;	// Начинает с заданного пользователем горизонта
    double nodes = 0;	// Оценивает общий объем сохраняемых профилей
    for (std::size_t i = 0; i < sectionCount; ++i) { nodes += metalCells(p, i) + 2; }	// Складывает узлы пяти металлических сечений
    const double limit = std::min(20000.0, std::floor(220.0 * 1024 * 1024 / (nodes * sizeof(float))) - 1);	// Оставляет резерв памяти для служебных данных
    if (p.autoTime) { duration = std::min(360.0 * 3600, limit * p.step); }	// Ограничивает автоматический поиск памятью и временем
    const auto solve = [&](std::size_t i, double horizon, bool search) {	// Формирует и решает одно радиальное сечение
        const auto& metal = p.materials[0];	// Выбирает единственный сплав всего валка
        const auto& paint = p.materials[1];	// Выбирает теплоизоляционную краску
        const auto& iron = p.materials[2];	// Выбирает материал кокиля и опок
        const auto& form = p.materials[i == 2 ? 2 : 3];	// Окружает бочку кокилем остальные участки песком
        const double radius = p.dimensions[i].diameter / 2000.0;	// Вычисляет радиус металлического отделения
        std::vector<CoolingLayer> layers{{radius, metal.conductivity, metal.density, metal.heatCapacity, p.metalTemperature, metalCells(p, i)}};	// Создает металлические кольца заданной сетки
        if (p.coating > 0) {	// Добавляет физически присутствующее покрытие
            layers.push_back({p.coating / 1000, paint.conductivity, paint.density, paint.heatCapacity, p.paintTemperature, 2});	// Описывает параметры текущего действия
        }	// Завершает текущий блок обработки
        layers.push_back({(i == 2 ? p.chill : p.sand) / 1000, form.conductivity, form.density, form.heatCapacity, (i == 2 ? p.ironTemperature : p.sandTemperature), 32});	// Описывает параметры текущего действия
        layers.push_back({(i == 2 ? p.barrelFlask : p.flask) / 1000, iron.conductivity, iron.density, iron.heatCapacity, p.ironTemperature, 8});	// Описывает параметры текущего действия
        const CoolingBoundary boundary{p.airTemperature, p.externalTransfer, horizon, p.step, 0, 0, true, radius + p.coating / 1000 + (i == 2 ? 0 : p.sand / 1000), search};	// Описывает параметры текущего действия
        return solveCooling(layers, boundary, p.phase, std::nullopt, stop);	// Описывает параметры текущего действия
    };	// Завершает объявление структуры или набора данных
    if (p.autoTime) {	// Ищет момент завершения каждого отделения
        double latestFrame = 0;	// Собирает общий момент завершения поиска
        for (std::size_t i = 0; i < sectionCount; ++i) {	// Выполняет пять ограниченных поисков
            auto history = solve(i, duration, true);	// Останавливает сечение после достижения солидуса
            if (!history.completedAt) { throw std::runtime_error("Авто: затвердевание не достигнуто в безопасном пределе. Задайте время числом или измените условия."); }	// Не выдает защитный предел за полное затвердевание
            latestFrame = std::max(latestFrame, history.samples.back().seconds);	// Учитывает самое медленное отделение
        }	// Завершает текущий блок обработки
        duration = latestFrame;	// Устанавливает единый горизонт всех температурных историй
    }	// Завершает текущий блок обработки
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Сохраняет согласованные истории всех отделений
        pending.histories[i] = solve(i, duration, false);	// Продолжает охлаждение уже затвердевших сечений
    }	// Завершает текущий блок обработки
    pending.configured = true;	// Отмечает пользовательскую постановку
    *this = std::move(pending);	// Публикует только полностью рассчитанный набор
}	// Завершает текущий блок обработки
