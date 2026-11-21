#pragma once	// Защищает заголовок расчета формы от повторного подключения
#include "project.h"	// Подключает расчет и фазовые данные одного цилиндра
#include "mold_dimensions.h"	// Подключает независимую от Windows геометрию формы
struct PhaseVolumes {	// Хранит абсолютные объемы двух фаз металла
    double solid;	// Хранит объем твердой фазы в кубометрах
    double liquid;	// Хранит объем жидкой фазы в кубометрах
};	// Завершает объявление структуры или набора данных
double cylinderVolume(const SectionDimensions& dimensions);	// Вычисляет геометрический объем цилиндрического отделения
PhaseVolumes phaseVolumes(const SectionDimensions& dimensions, const CoolingSample& sample);	// Вычисляет объемы фаз в выбранном кадре
class MoldCooling {	// Хранит согласованные истории всех отделений формы
public:	// Открывает расчет и чтение историй
    void calculate(const Project& project, std::stop_token stop = {});	// Рассчитывает заданный проект с возможностью отмены
    bool refresh(const MoldDimensions& dimensions, bool circulation = false);	// Пересчитывает отделения с изменившимися диаметрами
    std::size_t sampleCount() const;	// Возвращает число кадров общей шкалы
    const CoolingResult& section(std::size_t index) const;	// Возвращает историю выбранного отделения
    PhaseVolumes totalVolumes(const MoldDimensions& dimensions, std::size_t frame) const;	// Суммирует фазовые объемы формы
    std::optional<double> completedAt() const;	// Находит момент полного затвердевания всех отделений
private:	// Хранит данные только для внутреннего обновления
    std::array<CoolingResult, demoDimensions.size()> histories{};	// Сохраняет результаты по номерам сверху вниз
    bool configured = false;	// Различает пользовательские и демонстрационные истории
    bool hasCirculation = false;	// Запоминает режим сохраненных историй
    std::array<int, sectionCount> lengths{};	// Запоминает длины для модели движения
    std::array<int, demoDimensions.size()> diameters{};	// Запоминает диаметры рассчитанных отделений
};	// Завершает объявление структуры или набора данных
