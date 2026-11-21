#pragma once	// Защищает описание проекта от повторного подключения
#include "cooling.h"	// Подключает тепловую модель и фазовый переход
#include "mold_dimensions.h"	// Подключает пять отделений монолитного валка
struct Material {	// Хранит постоянные свойства одного материала
    double density;	// Задаёт плотность в килограммах на кубометр
    double heatCapacity;	// Задаёт теплоёмкость в джоулях на килограмм-кельвин
    double conductivity;	// Задаёт теплопроводность в ваттах на метр-кельвин
    bool operator==(const Material&) const = default;	// Сравнивает свойства при работе с файлами
};	// Завершает свойства материала
struct Project {	// Описывает монолитный валок и окружающую форму
    MoldDimensions dimensions = demoDimensions;	// Хранит D1–D5 и осевые размеры L1–L5
    double chill = 420;	// Задаёт толщину кокиля SK в миллиметрах
    double coating = 1;	// Задаёт толщину краски SC в миллиметрах
    double barrelFlask = 30;	// Задаёт стенку опоки бочки SOP
    double sand = 145;	// Задаёт стенку песчаной формы SFS
    double flask = 20;	// Задаёт стенку опоки шеек SOS
    double pipeDiameter = 110;	// Задаёт внутренний диаметр литниковой трубы DS
    std::array<Material, 4> materials{{{7000, 750, 30}, {2900, 400, 2.5}, {7800, 500, 45}, {1550, 800, 1.1}}};	// Хранит металл краску чугун и песчаную форму
    PhaseChange phase{1418, 1485, 249065};	// Переносит фазовые данные металла со снимка
    double metalTemperature = 1585;	// Задаёт температуру металла после заполнения
    double formTemperature = 200;	// Задаёт начальную температуру всех материалов формы
    double airTemperature = 20;	// Задаёт температуру окружающего воздуха
    double externalTransfer = 3;	// Задаёт наружную теплоотдачу со снимка
    double hours = 25;	// Переводит 1500 минут в часы
    double step = 30;	// Задаёт шаг времени в секундах
    double diameterStep = 1;	// Задаёт максимальный шаг по диаметру
    bool operator==(const Project&) const = default;	// Сравнивает входы с рассчитанным результатом
};	// Завершает воспроизводимые входные данные
void validateProject(const Project& project);	// Проверяет диапазоны сетку и расход памяти
int metalCells(const Project& project, std::size_t section);	// Определяет число металлических колец
double outerDiameter(const Project& project);	// Рассчитывает максимальный внешний диаметр формы
double totalLength(const Project& project);	// Суммирует пять осевых размеров
std::array<double, sectionCount> filledSections(const Project& project, double progress);	// Вычисляет наглядное заполнение снизу вверх
