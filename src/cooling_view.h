#pragma once																										// Защищает заголовок от повторного подключения
#include "mold_view.h"																								// Подключает безопасные настройки заголовков Windows
#include "mold_cooling.h"																							// Подключает истории и геометрию всех отделений
void drawCooling(HDC device, const RECT& area, const CoolingResult& result, const SectionDimensions& dimensions,	// Получает диаметр и высоту отделения
    std::size_t frame);																								// Показывает состояние фаз в выбранном кадре
void drawMoldOverview(HDC device, const RECT& area, const MoldCooling& cooling,										// Рисует сводку затвердевания всей формы
    const MoldDimensions& dimensions, std::size_t frame);															// Показывает объёмы на общей шкале времени
