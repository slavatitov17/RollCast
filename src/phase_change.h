#pragma once	// Защищает заголовок от повторного подключения
#include <array>	// Хранит данные трех фазовых зон фиксированного размера
#include <span>	// Передает радиальный профиль без копирования
struct PhaseChange {	// Задает температурный интервал и скрытую теплоту
    double solidus;	// Задает температуру полного затвердевания в градусах Цельсия
    double liquidus;	// Задает температуру полностью жидкого металла
    double latentHeat;	// Задает скрытую теплоту в джоулях на килограмм
    bool operator==(const PhaseChange&) const = default;	// Сравнивает параметры фазового перехода
    double liquidShare(double temperature) const;	// Вычисляет местную объемную долю жидкости
    double liquidSlope(double temperature) const;	// Вычисляет производную доли по температуре
};	// Завершает объявление структуры или набора данных
inline constexpr PhaseChange demoPhase{1320, 1450, 270000};	// Задает учебные параметры фазового перехода
struct RadialTemperature {	// Хранит температуру в узле восстановленного профиля
    double radius;	// Задает радиус узла в метрах
    double temperature;	// Задает температуру узла в градусах Цельсия
};	// Завершает объявление структуры или набора данных
struct PhaseZone {	// Описывает одну геометрическую зону сечения
    double thickness;	// Хранит суммарную радиальную толщину зоны в метрах
    double volumeShare;	// Хранит отношение объема зоны к объему цилиндра
};	// Завершает объявление структуры или набора данных
struct PhaseSummary {	// Хранит геометрические зоны и объемы отдельных фаз
    std::array<PhaseZone, 3> zones;	// Перечисляет твердую двухфазную и жидкую зоны
    double liquidVolumeShare;	// Учитывает жидкость во всей бочке
    double hottestTemperature;	// Хранит температуру последней затвердевающей точки
};	// Завершает объявление структуры или набора данных
PhaseSummary summarizePhases(std::span<const RadialTemperature> profile, PhaseChange phase);	// Интегрирует доли и границы зон
