#pragma once																				// Защищает заголовок от повторного подключения
#include "mold_view.h"																		// Подключает безопасные настройки заголовков Windows
#include "cooling.h"																		// Подключает историю решения тепловой задачи
void drawCooling(HDC device, const RECT& area, const CoolingResult& result, int diameter);	// Рисует две температурные кривые
