#pragma once																				// Защищает заголовок от повторного подключения
#include <span>																				// Передаёт слои модели без копирования массива
#include <vector>																			// Хранит рассчитанные температуры во времени
struct CoolingLayer {																		// Задаёт кольцевой слой радиальной расчётной области
    double thickness;																		// Задаёт толщину слоя в метрах
    double conductivity;																	// Задаёт теплопроводность в ваттах на метр-кельвин
    double density;																			// Задаёт плотность в килограммах на кубический метр
    double heatCapacity;																	// Задаёт удельную теплоёмкость в джоулях на килограмм-кельвин
    double initialTemperature;																// Задаёт начальную температуру слоя в градусах Цельсия
    int cells;																				// Задаёт число конечных объёмов по радиусу слоя
};																							// Завершает описание слоя
struct CoolingBoundary {																	// Хранит настройки теплообмена и времени
    double airTemperature;																	// Задаёт температуру воздуха в градусах Цельсия
    double heatTransfer;																	// Задаёт теплоотдачу в ваттах на квадратный метр-кельвин
    double duration;																		// Задаёт длительность расчёта в секундах
    double maxStep;																			// Ограничивает шаг расчёта в секундах
};																							// Завершает описание настроек
struct CoolingSample {																		// Хранит температуры двух точек металла
    double seconds;																			// Отсчитывает время от конца заполнения формы
    double axisTemperature;																	// Хранит температуру металла на оси бочки
    double surfaceTemperature;																// Хранит температуру металла на боковой стенке
};																							// Завершает описание отсчёта температуры
struct CoolingResult {																		// Объединяет историю температур и проверку баланса
    std::vector<CoolingSample> samples;														// Хранит начальный и последующие отсчёты
    double relativeEnergyError;																// Хранит относительную невязку теплового баланса
};																							// Завершает описание результата
CoolingResult solveCooling(std::span<const CoolingLayer> layers, CoolingBoundary boundary);	// Рассчитывает теплообмен концентрических слоёв
CoolingResult demonstrationCooling(int diameterMm);											// Рассчитывает учебный пример для диаметра бочки
