#pragma once	// Защищает интерфейс графиков от повторного подключения
#include "mold_view.h"	// Подключает параметры формы и функции Windows
#include "project_io.h"	// Подключает согласованные рассчитанные проекты
struct CurveCache {	// Хранит подготовленные линии одного отделения
    RECT plot{};	// Запоминает область масштабирования кривых
    const CoolingResult* source = nullptr;	// Различает историю текущего проекта
    const CoolingResult* baseline = nullptr;	// Различает закрепленную историю сравнения
    std::array<std::vector<POINT>, 6> curves;	// Описывает параметры текущего действия
};	// Завершает объявление структуры или набора данных
using ChartCache = std::array<CurveCache, sectionCount>;	// Разделяет кэши пяти отделений
std::array<RECT, sectionCount> chartAreas(const RECT& area, int selection);	// Размещает только выбранное отделение
void drawSingleChart(HDC device, const RECT& area, const Project& project, const Run* run, const Run* baseline, std::size_t section, std::size_t frame, CurveCache& cache, double filling = 1, int zoom = 100);	// Описывает параметры текущего действия
void drawMoldDashboard(HDC device, const RECT& area, const Project& project, const Run* run, const Run* baseline, std::size_t frame, double filling, int selection, ChartCache& cache, int zoom = 100);	// Описывает параметры текущего действия
