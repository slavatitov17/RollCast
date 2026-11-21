#include "phase_change.h"	// Подключает параметры и показатели фазового перехода
#include <algorithm>	// Предоставляет ограничение доли и сортировку границ
#include <cmath>	// Восстанавливает координаты радиальных границ зон
double PhaseChange::liquidShare(double temperature) const {	// Находит долю жидкости при заданной температуре
    return std::clamp((temperature - solidus) / (liquidus - solidus), 0.0, 1.0);	// Линейно меняет долю между порогами
}	// Завершает текущий блок обработки
double PhaseChange::liquidSlope(double temperature) const {	// Находит наклон закона фазового перехода
    return temperature > solidus && temperature < liquidus ? 1 / (liquidus - solidus) : 0;	// Берет наклон только в двухфазной области
}	// Завершает текущий блок обработки
PhaseSummary summarizePhases(std::span<const RadialTemperature> profile, PhaseChange phase) {	// Преобразует профиль в объемы и толщины
    PhaseSummary result{};	// Начинает суммирование с нулевых объемов
    result.hottestTemperature = profile.front().temperature;	// Учитывает температуру на оси
    const double radius = profile.back().radius;	// Берет наружный радиус металла
    for (std::size_t i = 1; i < profile.size(); ++i) {	// Обходит соседние узлы радиального профиля
        const auto& inner = profile[i - 1];	// Берет внутреннюю точку интервала
        const auto& outer = profile[i];	// Берет наружную точку интервала
        result.hottestTemperature = std::max(result.hottestTemperature, outer.temperature);	// Ищет самый горячий узел профиля
        const double delta = outer.temperature - inner.temperature;	// Находит изменение температуры между узлами
        std::array<double, 4> cuts{0, 1};	// Хранит относительные координаты концов интервала
        std::size_t count = 2;	// Учитывает два обязательных конца интервала
        if (delta != 0) {	// Ищет пересечения порогов при изменении температуры
            for (const double threshold : {phase.solidus, phase.liquidus}) {	// Проверяет границы обеих фазовых зон
                const double crossing = (threshold - inner.temperature) / delta;	// Находит положение температурного порога
                if (crossing > 0 && crossing < 1) {	// Принимает только внутреннее пересечение
                    cuts[count++] = crossing;	// Добавляет точку разбиения интервала
                }	// Завершает текущий блок обработки
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        std::sort(cuts.begin(), cuts.begin() + count);	// Упорядочивает границы по площади сечения
        const double r0Squared = inner.radius * inner.radius;	// Вычисляет квадрат внутреннего радиуса
        const double width = (outer.radius - inner.radius) * (outer.radius + inner.radius);	// Находит ширину по квадрату радиуса
        for (std::size_t j = 1; j < count; ++j) {	// Суммирует интервалы без внутренних фазовых границ
            const double start = cuts[j - 1];	// Берет относительное начало интервала
            const double end = cuts[j];	// Берет относительный конец интервала
            const double first = inner.temperature + delta * start;	// Находит температуру начала интервала
            const double last = inner.temperature + delta * end;	// Находит температуру конца интервала
            const double middle = (first + last) / 2;	// Находит температуру середины площадного интервала
            const int zone = middle <= phase.solidus ? 0 : middle >= phase.liquidus ? 2 : 1;	// Определяет геометрическую зону интервала
            const double innerRadius = std::sqrt(r0Squared + width * start);	// Восстанавливает внутренний радиус интервала
            const double outerRadius = std::sqrt(r0Squared + width * end);	// Восстанавливает наружный радиус интервала
            const double volume = width * (end - start) / (radius * radius);	// Считает долю кольца в объеме бочки
            result.zones[zone].thickness += outerRadius - innerRadius;	// Прибавляет вклад в толщину зоны
            result.zones[zone].volumeShare += volume;	// Прибавляет вклад в геометрический объем
            result.liquidVolumeShare += volume * (phase.liquidShare(first) + phase.liquidShare(last)) / 2;	// Интегрирует линейную долю жидкости по объему
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    result.liquidVolumeShare = std::clamp(result.liquidVolumeShare, 0.0, 1.0);	// Устраняет погрешность округления доли жидкости
    return result;	// Возвращает состояние зон и отдельных фаз
}	// Завершает текущий блок обработки
