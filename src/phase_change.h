#pragma once																					// Защищает заголовок от повторного подключения
#include <array>																				// Хранит данные трёх фазовых зон фиксированного размера
#include <span>																					// Передаёт радиальный профиль без копирования
struct PhaseChange {																			// Задаёт температурный интервал и скрытую теплоту
    double solidus;																				// Задаёт температуру полного затвердевания в градусах Цельсия
    double liquidus;																			// Задаёт температуру полностью жидкого металла
    double latentHeat;																			// Задаёт скрытую теплоту в джоулях на килограмм
    double liquidShare(double temperature) const;												// Вычисляет местную объёмную долю жидкости
    double liquidSlope(double temperature) const;												// Вычисляет производную доли по температуре
};																								// Завершает описание закона затвердевания
inline constexpr PhaseChange demoPhase{1320, 1450, 270000};										// Задаёт учебные параметры фазового перехода
struct RadialTemperature {																		// Хранит температуру в узле восстановленного профиля
    double radius;																				// Задаёт радиус узла в метрах
    double temperature;																			// Задаёт температуру узла в градусах Цельсия
};																								// Завершает описание узла профиля
struct PhaseZone {																				// Описывает одну геометрическую зону сечения
    double thickness;																			// Хранит суммарную радиальную толщину зоны в метрах
    double volumeShare;																			// Хранит отношение объёма зоны к объёму цилиндра
};																								// Завершает описание геометрии зоны
struct PhaseSummary {																			// Хранит геометрические зоны и объёмы отдельных фаз
    std::array<PhaseZone, 3> zones;																// Перечисляет твёрдую двухфазную и жидкую зоны
    double liquidVolumeShare;																	// Учитывает жидкость во всей бочке
    double hottestTemperature;																	// Хранит температуру последней затвердевающей точки
};																								// Завершает описание фазового состояния
PhaseSummary summarizePhases(std::span<const RadialTemperature> profile, PhaseChange phase);	// Интегрирует доли и границы зон
