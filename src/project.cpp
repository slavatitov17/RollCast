#include "project.h"	// Подключает параметры монолитного валка
#include <algorithm>	// Ограничивает степень заполнения и размеры
#include <cmath>	// Проверяет числа и округляет сетку
#include <numbers>	// Предоставляет постоянную пи
#include <stdexcept>	// Сообщает о недопустимых входных данных
namespace {	// Скрывает внутреннюю проверку диапазона
void range(double value, double minimum, double maximum) {	// Проверяет одно конечное числовое значение
    if (!std::isfinite(value) || value < minimum || value > maximum) {	// Отклоняет недопустимые числовые параметры
        throw std::invalid_argument("Параметр выходит за допустимый диапазон");	// Объясняет отказ применения значений
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
int metalCells(const Project& p, std::size_t section) {	// Переводит диаметральный шаг в число колец
    return static_cast<int>(std::ceil(p.dimensions.at(section).diameter / p.diameterStep));	// Делит радиус с шагом не больше ΔD/2
}	// Завершает текущий блок обработки
double outerDiameter(const Project& p) {	// Рассчитывает внешний размер формы
    double diameter = 0;	// Начинает поиск наибольшего наружного диаметра
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Находит вместимость каждого цилиндра
        const double wall = p.coating + (i == 2 ? p.chill + p.barrelFlask : p.sand + p.flask);	// Складывает действующие радиальные слои
        diameter = std::max(diameter, p.dimensions[i].diameter + 2 * wall);	// Учитывает стенки с обеих сторон
    }	// Завершает текущий блок обработки
    return diameter;	// Возвращает D0 без литниковой трубы
}	// Завершает текущий блок обработки
double totalLength(const Project& p) {	// Вычисляет общий осевой размер L0
    double length = 0;	// Начинает сумму длин с нуля
    for (const auto& d : p.dimensions) { length += d.height; }	// Складывает длины всех пяти отделений
    return length;	// Возвращает осевую длину в миллиметрах
}	// Завершает текущий блок обработки
void validateProject(const Project& p) {	// Проверяет согласованность расчетного проекта
    range(p.diameterStep, 0.1, 1000);	// Проверяет положительный шаг по диаметру
    double nodes = 0;	// Оценивает сохраняемую размерность профилей
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Находит вместимость каждого цилиндра
        range(p.dimensions[i].diameter, 1, 9999);	// Ограничивает целочисленный диаметр в миллиметрах
        range(p.dimensions[i].height, 1, 9999);	// Ограничивает осевую длину отделения
        range(metalCells(p, i), 2, 4096);	// Ограничивает сетку металлического сечения
        nodes += metalCells(p, i) + 2;	// Учитывает центры колец ось и стенку
    }	// Завершает текущий блок обработки
    range(p.chill, 0.1, 2000); range(p.coating, 0, 100);	// Проверяет кокиль и необязательное покрытие
    range(p.barrelFlask, 0.1, 1000); range(p.flask, 0.1, 1000);	// Проверяет стенки обеих разновидностей опок
    range(p.sand, 0.1, 2000); range(p.pipeDiameter, 1, 1000);	// Проверяет форму и литниковую трубу
    for (const auto& m : p.materials) {	// Проверяет четыре материала расчетной области
        range(m.density, 1, 30000); range(m.heatCapacity, 1, 100000);	// Проверяет запасание энергии материалом
        range(m.conductivity, 0.001, 10000);	// Проверяет способность материала проводить теплоту
    }	// Завершает текущий блок обработки
    range(p.phase.solidus, -273, 4000); range(p.phase.liquidus, -273, 4000);	// Проверяет конечность фазовых температур
    range(p.phase.latentHeat, 1, 10000000);	// Проверяет положительную теплоту кристаллизации
    range(p.metalTemperature, p.phase.liquidus, 4000);	// Требует первоначально жидкий металл
    range(p.formTemperature, -273, 4000); range(p.airTemperature, -273, 4000);	// Проверяет начальные температуры внешних материалов
    range(p.externalTransfer, 0, 100000);	// Разрешает наружную теплоизоляцию нулем
    range(p.hours, 0.001, 360); range(p.step, 0.01, 3600);	// Проверяет горизонт и шаг времени
    range(p.paintTemperature, -273, 4000); range(p.ironTemperature, -273, 4000); range(p.sandTemperature, -273, 4000);	// Проверяет начальные температуры трех материалов
    range(p.ironSolidus, -273, 4000); range(p.ironLiquidus, p.ironSolidus + 0.001, 4000);	// Проверяет порядок контрольных температур оснастки
    const double frames = p.autoTime ? 2 : std::ceil(p.hours * 3600 / p.step) + 1;	// Ограничивает фиксированный горизонт до выделения памяти
    if (p.phase.solidus >= p.phase.liquidus || frames > 20001 || frames * nodes * sizeof(float) > 256 * 1024 * 1024) {	// Ограничивает фазовый интервал и память истории
        throw std::invalid_argument("Проверьте солидус, ликвидус, шаг сетки и время расчета");	// Объясняет несовместимость выбранных параметров
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
std::array<double, sectionCount> filledSections(const Project& p, double progress) {	// Вычисляет наглядное заполнение монолитной отливки
    std::array<double, sectionCount> fractions{};	// Подготавливает объемы пяти отделений
    double total = 0;	// Исключает литниковую систему из суммы
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Находит вместимость каждого цилиндра
        const double radius = p.dimensions[i].diameter / 2000.0;	// Переводит диаметр в радиус метрами
        fractions[i] = std::numbers::pi * radius * radius * p.dimensions[i].height / 1000;	// Вычисляет объем отделения в кубометрах
        total += fractions[i];	// Добавляет объем к вместимости валка
    }	// Завершает текущий блок обработки
    double remaining = total * std::clamp((progress - 0.2) / 0.8, 0.0, 1.0);	// Оставляет начало сценария литниковому пути
    for (std::size_t i = sectionCount; i-- > 0;) {	// Заполняет отделения от нижней шейки вверх
        const double filled = std::min(remaining, fractions[i]);	// Ограничивает поступление вместимостью отделения
        remaining -= filled; fractions[i] = filled / fractions[i];	// Вычисляет долю заполнения цилиндра
    }	// Завершает текущий блок обработки
    return fractions;	// Возвращает согласованные доли заполнения
}	// Завершает текущий блок обработки
double totalVolume(const Project& p) {	// Рассчитывает общий объем металла в кубических метрах
    double volume = 0;	// Начинает сумму объемов пяти расчетных цилиндров
    for (const auto& d : p.dimensions) {	// Учитывает все участки сверху вниз
        const double radius = d.diameter / 2000.0;	// Переводит диаметр миллиметров в радиус метров
        volume += std::numbers::pi * radius * radius * d.height / 1000.0;	// Умножает площадь сечения на осевую длину
    }	// Завершает текущий блок обработки
    return volume;	// Исключает металл в литниковых каналах
}	// Завершает текущий блок обработки
