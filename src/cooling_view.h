#pragma once	// Защищает интерфейс графиков от повторного подключения
#include "mold_view.h"	// Подключает параметры формы и функции Windows
#include "project_io.h"	// Подключает согласованные рассчитанные проекты
#include "layer_metrics.h"	// Подключает два показателя готового состояния
struct CurveCache {	// Хранит подготовленные линии одного отделения
    RECT plot{};	// Запоминает область масштабирования кривых
    const CoolingResult* source = nullptr;	// Различает историю текущего проекта
    LayerMetric metric = LayerMetric::volume;	// Запоминает показатель подготовленных кривых
    int zoom = 0;	// Запоминает масштаб подготовленных кривых
    std::array<std::vector<POINT>, 3> curves;	// Настраивает отображение текущего элемента
};	// Завершает объявление структуры или набора данных
using ChartCache = std::array<CurveCache, sectionCount>;	// Разделяет кэши пяти отделений
std::array<RECT, sectionCount> chartAreas(const RECT& area, int selection);	// Размещает только выбранное отделение
void drawSingleChart(HDC device, const RECT& area, const Project& project, const Run* run, std::size_t section, std::size_t frame, CurveCache& cache, int zoom = 100, LayerMetric metric = LayerMetric::volume, unsigned layers = 7);	// Настраивает отображение текущего элемента
void drawMoldDashboard(HDC device, const RECT& area, const Project& project, const Run* run, std::size_t frame, double filling, int selection, ChartCache& cache, int zoom = 100, LayerMetric metric = LayerMetric::volume, unsigned layers = 7);	// Настраивает отображение текущего элемента
