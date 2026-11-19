#pragma once																			// Защищает заголовок от повторного подключения
#include "mold_view.h"																	// Подключает безопасные настройки заголовков Windows
#include "mold_cooling.h"																// Подключает истории и геометрию всех отделений
void drawMoldDashboard(HDC device, const RECT& area, const MoldCooling& cooling,		// Принимает результаты и область общего экрана
    const MoldDimensions& dimensions, std::size_t frame);								// Показывает сводку и четыре графика
