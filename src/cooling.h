#pragma once																																					// Защищает заголовок от повторного подключения
#include "phase_change.h"																																		// Подключает параметры и геометрию фазовых зон
#include <optional>																																				// Хранит момент полного затвердевания при достижении солидуса
#include <span>																																					// Передаёт слои модели без копирования массива
#include <stop_token>																																		// Позволяет прервать фоновый расчёт
#include <vector>																																				// Хранит рассчитанные температуры во времени
struct CoolingLayer {																																			// Задаёт кольцевой слой радиальной расчётной области
    double thickness;																																			// Задаёт толщину слоя в метрах
    double conductivity;																																		// Задаёт теплопроводность в ваттах на метр-кельвин
    double density;																																				// Задаёт плотность в килограммах на кубический метр
    double heatCapacity;																																		// Задаёт удельную теплоёмкость в джоулях на килограмм-кельвин
    double initialTemperature;																																	// Задаёт начальную температуру слоя в градусах Цельсия
    int cells;																																					// Задаёт число конечных объёмов по радиусу слоя
};																																								// Завершает описание слоя
struct CoolingBoundary {																																		// Хранит настройки теплообмена и времени
    double airTemperature;																																		// Задаёт температуру воздуха в градусах Цельсия
    double heatTransfer;																																		// Задаёт теплоотдачу в ваттах на квадратный метр-кельвин
    double duration;																																			// Задаёт длительность расчёта в секундах
    double maxStep;																																				// Ограничивает шаг расчёта в секундах
    double metalContact = 0;																																// Задаёт конечный контакт металла с формой
    double formContact = 0;																																	// Задаёт конечный контакт материалов формы
    bool recordProfile = false;																																// Включает профиль для окраски фаз на схеме
};																																								// Завершает описание настроек
struct MeltCirculation {																																		// Задаёт сокращённую модель циркуляционного обмена
    double length;																																				// Задаёт осевую длину отделения в метрах
    double viscosity = 0.006;																																	// Задаёт динамическую вязкость по Chaulet и соавторам
    double expansion = 1.1e-4;																																	// Задаёт объёмное расширение по той же статье
};																																								// Завершает описание параметров движения
struct CoolingSample {																																			// Хранит температуры двух точек металла
    double seconds;																																				// Отсчитывает время от конца заполнения формы
    double axisTemperature;																																		// Хранит температуру металла на оси бочки
    double surfaceTemperature;																																	// Хранит температуру металла на боковой стенке
    PhaseSummary phase{};																																		// Хранит распределение фаз в текущий момент
    double circulationSpeed = 0;																																// Хранит масштаб скорости учебной циркуляции
    std::vector<RadialTemperature> profile{};																												// Хранит радиальный профиль для цветной схемы
};																																								// Завершает описание отсчёта температуры
struct CoolingResult {																																			// Объединяет историю температур и проверку баланса
    std::vector<CoolingSample> samples;																															// Хранит начальный и последующие отсчёты
    double relativeEnergyError;																																	// Хранит относительную невязку теплового баланса
    std::optional<double> completedAt;																															// Хранит время полного затвердевания при его достижении
};																																								// Завершает описание результата
CoolingResult solveCooling(std::span<const CoolingLayer> layers, CoolingBoundary boundary,																		// Принимает слои и внешнее условие
    std::optional<PhaseChange> phase = std::nullopt, std::optional<MeltCirculation> circulation = std::nullopt, std::stop_token stop = {});					// Включает скрытую теплоту при задании фазы
CoolingResult demonstrationCooling(int diameterMm, std::optional<PhaseChange> phase = std::nullopt, std::optional<MeltCirculation> circulation = std::nullopt);	// Рассчитывает учебный пример для диаметра бочки
