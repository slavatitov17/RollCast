#pragma once																					// Защищает заголовок расчёта формы от повторного подключения
#include "cooling.h"																			// Подключает расчёт и фазовые данные одного цилиндра
#include "mold_dimensions.h"																	// Подключает независимую от Windows геометрию формы
struct PhaseVolumes {																			// Хранит абсолютные объёмы двух фаз металла
    double solid;																				// Хранит объём твёрдой фазы в кубометрах
    double liquid;																				// Хранит объём жидкой фазы в кубометрах
};																								// Завершает описание фазовых объёмов
double cylinderVolume(const SectionDimensions& dimensions);										// Вычисляет геометрический объём цилиндрического отделения
PhaseVolumes phaseVolumes(const SectionDimensions& dimensions, const CoolingSample& sample);	// Вычисляет объёмы фаз в выбранном кадре
class MoldCooling {																				// Хранит согласованные истории всех отделений формы
public:																							// Предоставляет расчёт и чтение общей шкалы времени
    bool refresh(const MoldDimensions& dimensions, bool circulation = false);					// Пересчитывает отделения с изменившимися диаметрами
    std::size_t sampleCount() const;															// Возвращает число кадров общей шкалы
    const CoolingResult& section(std::size_t index) const;										// Возвращает историю выбранного отделения
    PhaseVolumes totalVolumes(const MoldDimensions& dimensions, std::size_t frame) const;		// Суммирует фазовые объёмы формы
    std::optional<double> completedAt() const;													// Находит момент полного затвердевания всех отделений
private:																						// Хранит данные только для внутреннего обновления
    std::array<CoolingResult, demoDimensions.size()> histories{};								// Сохраняет результаты по отделениям снизу вверх
    bool hasCirculation = false;																// Запоминает режим сохранённых историй
    std::array<int, 4> lengths{};																// Запоминает длины для модели движения
    std::array<int, demoDimensions.size()> diameters{};											// Запоминает диаметры рассчитанных отделений
};																								// Завершает описание расчёта формы
