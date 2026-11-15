#pragma once																			// Защищает заголовок от повторного подключения
#ifndef NOMINMAX																		// Проверяет настройку имен стандартных функций
#define NOMINMAX																		// Отключает макросы Windows для максимума и минимума
#endif																					// Завершает выбор настройки заголовка Windows
#include <windows.h>																	// Объявляет контекст рисования и прямоугольник Windows
#include <array>																		// Предоставляет массив отделений фиксированного размера
struct SectionDimensions {																// Хранит размеры одного цилиндрического отделения
    int diameter;																		// Задаёт диаметр в миллиметрах
    int height;																			// Задаёт высоту в миллиметрах
};																						// Завершает описание размеров отделения
using MoldDimensions = std::array<SectionDimensions, 4>;								// Упорядочивает отделения формы снизу вверх
inline constexpr MoldDimensions demoDimensions{{										// Задаёт условные исходные размеры
    {520, 820},																			// Нижняя шейка
    {920, 1310},																		// Бочка валка
    {560, 765},																			// Верхняя шейка
    {460, 704},																			// Прибыль
}};																						// Завершает набор демонстрационных размеров
void drawMold(HDC device, const RECT& area, const MoldDimensions& dimensions);			// Рисует продольный разрез формы
