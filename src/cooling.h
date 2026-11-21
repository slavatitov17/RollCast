#pragma once	// Защищает заголовок от повторного подключения
#include "phase_change.h"	// Подключает параметры и геометрию фазовых зон
#include <optional>	// Хранит момент полного затвердевания при достижении солидуса
#include <span>	// Передает слои модели без копирования массива
#include <stop_token>	// Позволяет прервать фоновый расчет
#include <vector>	// Хранит рассчитанные температуры во времени
struct CoolingLayer {	// Задает кольцевой слой радиальной расчетной области
    double thickness;	// Задает толщину слоя в метрах
    double conductivity;	// Задает теплопроводность в ваттах на метр-кельвин
    double density;	// Задает плотность в килограммах на кубический метр
    double heatCapacity;	// Задает удельную теплоемкость в джоулях на килограмм-кельвин
    double initialTemperature;	// Задает начальную температуру слоя в градусах Цельсия
    int cells;	// Задает число конечных объемов по радиусу слоя
};	// Завершает объявление структуры или набора данных
struct CoolingBoundary {	// Хранит настройки теплообмена и времени
    double airTemperature;	// Задает температуру воздуха в градусах Цельсия
    double heatTransfer;	// Задает теплоотдачу в ваттах на квадратный метр-кельвин
    double duration;	// Задает длительность расчета в секундах
    double maxStep;	// Ограничивает шаг расчета в секундах
    double metalContact = 0;	// Задает конечный контакт металла с формой
    double formContact = 0;	// Задает конечный контакт материалов формы
    bool recordProfile = false;	// Включает профиль для окраски фаз на схеме
    double ironRadius = 1e100;	// Отмечает внутренний радиус контролируемой металлической оснастки
    bool stopWhenSolid = false;	// Разрешает остановку поиска после затвердевания
};	// Завершает объявление структуры или набора данных
struct MeltCirculation {	// Задает сокращенную модель циркуляционного обмена
    double length;	// Задает осевую длину отделения в метрах
    double viscosity = 0.006;	// Задает динамическую вязкость по Chaulet и соавторам
    double expansion = 1.1e-4;	// Задает объемное расширение по той же статье
};	// Завершает объявление структуры или набора данных
struct CoolingSample {	// Хранит температуры двух точек металла
    double seconds;	// Отсчитывает время от конца заполнения формы
    double axisTemperature;	// Хранит температуру металла на оси бочки
    double surfaceTemperature;	// Хранит температуру металла на боковой стенке
    PhaseSummary phase{};	// Хранит распределение фаз в текущий момент
    double circulationSpeed = 0;	// Хранит масштаб скорости учебной циркуляции
    std::vector<float> profile{};	// Хранит температуры профиля для цветной схемы
};	// Завершает объявление структуры или набора данных
struct CoolingResult {	// Объединяет историю температур и проверку баланса
    std::vector<CoolingSample> samples;	// Хранит начальный и последующие отсчеты
    double maxIronTemperature = -273;	// Хранит максимальный нагрев кокиля и опоки
    double relativeEnergyError;	// Хранит относительную невязку теплового баланса
    std::optional<double> completedAt;	// Хранит время полного затвердевания при его достижении
};	// Завершает объявление структуры или набора данных
CoolingResult solveCooling(std::span<const CoolingLayer> layers, CoolingBoundary boundary,	// Принимает слои и внешнее условие
    std::optional<PhaseChange> phase = std::nullopt, std::optional<MeltCirculation> circulation = std::nullopt, std::stop_token stop = {});	// Включает скрытую теплоту при задании фазы
CoolingResult demonstrationCooling(int diameterMm, std::optional<PhaseChange> phase = std::nullopt, std::optional<MeltCirculation> circulation = std::nullopt);	// Рассчитывает учебный пример для диаметра бочки
