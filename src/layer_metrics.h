#pragma once	// Настраивает отображение текущего элемента
#include "mold_cooling.h"	// Настраивает отображение текущего элемента
enum class LayerMetric { volume, thickness };	// Различает объем зоны и радиальную толщину
inline constexpr std::array<std::size_t, 3> legendOrder{2, 1, 0};	// Показывает жидкий двухфазный и твердый слои
inline constexpr const wchar_t* areaNumbers[]{L"1", L"2", L"3", L"4", L"5"};	// Подписывает участки номерами схемы
inline constexpr const wchar_t* layerNames[]{L"Твердый", L"Двухфазный", L"Жидкий"};	// Сохраняет соответствие имен внутренним индексам
inline double metricCapacity(const SectionDimensions& d, LayerMetric metric) {	// Определяет полный масштаб участка в выбранных единицах
    return metric == LayerMetric::volume ? cylinderVolume(d) : d.diameter / 2.0;	// Возвращает кубометры либо радиус в миллиметрах
}	// Завершает текущий блок обработки
inline double layerValue(const SectionDimensions& d, const CoolingSample& s, std::size_t zone, LayerMetric metric, bool filling = false, double filled = 1) {	// Извлекает готовый показатель без повторного расчета
    if (filling) {	// Описывает лишь заполненную часть цилиндрического участка
        return zone == 2 && filled > 0 ? metricCapacity(d, metric) * (metric == LayerMetric::volume ? filled : 1) : 0;	// Различает объем заполнения и радиальную толщину жидкости
    }	// Завершает текущий блок обработки
    return metric == LayerMetric::volume ? cylinderVolume(d) * s.phase.zones.at(zone).volumeShare : s.phase.zones.at(zone).thickness * 1000;	// Переводит долю объема либо толщину в единицы
}	// Завершает текущий блок обработки
