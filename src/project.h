#pragma once	// Защищает описание проекта от повторного подключения
#include "cooling.h"	// Подключает тепловую модель и фазовый переход
#include "mold_dimensions.h"	// Подключает пять отделений монолитного валка
struct Material {	// Хранит постоянные свойства одного материала
    double density;	// Задает плотность в килограммах на кубометр
    double heatCapacity;	// Задает теплоемкость в джоулях на килограмм-кельвин
    double conductivity;	// Задает теплопроводность в ваттах на метр-кельвин
    bool operator==(const Material&) const = default;	// Сравнивает свойства при работе с файлами
};	// Завершает объявление структуры или набора данных
struct Project {	// Описывает монолитный валок и окружающую форму
    MoldDimensions dimensions = demoDimensions;	// Хранит D1–D5 и осевые размеры L1–L5
    double chill = 220;	// Описывает параметры текущего действия
    double coating = 10;	// Описывает параметры текущего действия
    double barrelFlask = 30;	// Задает стенку опоки бочки SOP
    double sand = 145;	// Задает стенку песчаной формы SFS
    double flask = 20;	// Задает стенку опоки шеек SOS
    double pipeDiameter = 110;	// Задает внутренний диаметр литниковой трубы DS
    std::array<Material, 4> materials{{{7000, 750, 30}, {2900, 400, 2.5}, {7800, 500, 45}, {1550, 800, 1.1}}};	// Хранит металл краску чугун и песчаную форму
    PhaseChange phase{1418, 1485, 249065};	// Переносит фазовые данные металла со снимка
    double metalTemperature = 1585;	// Задает температуру металла после заполнения
    double formTemperature = 200;	// Задает начальную температуру всех материалов формы
    double airTemperature = 20;	// Задает температуру окружающего воздуха
    double externalTransfer = 3;	// Задает наружную теплоотдачу со снимка
    bool autoTime = true;	// Выбирает расчет до полного затвердевания
    double paintTemperature = 200;	// Задает начальную температуру покрытия
    double ironTemperature = 200;	// Задает начальную температуру кокиля и опоки
    double sandTemperature = 200;	// Задает начальную температуру песчаной формы
    double ironLiquidus = 1250;	// Задает верхний контрольный порог оснастки
    double ironSolidus = 1147;	// Задает нижний контрольный порог оснастки
    double hours = 25;	// Переводит 1500 минут в часы
    double step = 30;	// Задает шаг времени в секундах
    double diameterStep = 1;	// Задает максимальный шаг по диаметру
    bool operator==(const Project&) const = default;	// Сравнивает входы с рассчитанным результатом
};	// Завершает объявление структуры или набора данных
void validateProject(const Project& project);	// Проверяет диапазоны сетку и расход памяти
int metalCells(const Project& project, std::size_t section);	// Определяет число металлических колец
double outerDiameter(const Project& project);	// Рассчитывает максимальный внешний диаметр формы
double totalLength(const Project& project);	// Суммирует пять осевых размеров
std::array<double, sectionCount> filledSections(const Project& project, double progress);	// Вычисляет наглядное заполнение снизу вверх
