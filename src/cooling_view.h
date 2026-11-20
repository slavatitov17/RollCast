#pragma once																																				// Защищает интерфейс отображения результатов
#include "mold_view.h"																																		// Подключает геометрию формы и средства Windows
#include "project_io.h"																																		// Подключает рассчитанный проект и базу сравнения
struct CurveCache {																																			// Хранит подготовленные линии одного графика
    RECT plot{};																																			// Запоминает область масштабирования кривых
    const CoolingResult* source = nullptr;																													// Запоминает текущую историю отделения
    const CoolingResult* baseline = nullptr;																												// Запоминает закреплённую историю отделения
    std::array<std::vector<POINT>, 4> curves;																												// Сохраняет текущие и базовые температурные линии
};																																							// Завершает кэш одного графика
using ChartCache = std::array<CurveCache, 4>;																												// Хранит четыре подготовленные пары кривых
std::array<RECT, 4> chartAreas(const RECT& area, int selection);																							// Размещает один или все четыре графика
void drawMoldDashboard(HDC device, const RECT& area, const Run& run, const Run* baseline,																	// Получает текущий результат и базу сравнения
    std::size_t frame, double filling, int selection, ChartCache& cache);																					// Рисует таблицу и выбранные графики
void drawSingleChart(HDC device, const RECT& area, const Run& run, const Run* baseline,																		// Получает данные отдельного экспортируемого графика
    std::size_t section, std::size_t frame, CurveCache& cache, double filling = 1);																			// Рисует выбранное отделение без оконных элементов
