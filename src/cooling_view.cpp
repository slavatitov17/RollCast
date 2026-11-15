#include "cooling_view.h"																						// Подключает интерфейс температурного графика
#include <cmath>																								// Округляет координаты линий и текста
#include <cwchar>																								// Форматирует время и температуры для вывода
namespace {																										// Оставляет отрисовку подписей внутри модуля графика
constexpr COLORREF axisColor = RGB(23, 92, 150);																// Задаёт синий цвет осевой кривой
constexpr COLORREF surfaceColor = RGB(170, 75, 27);																// Задаёт тёплый цвет поверхностной кривой
void line(HDC device, int x, int y, int endX, int endY) {														// Рисует линию текущим пером
    MoveToEx(device, x, y, nullptr);																			// Ставит перо в начало линии
    LineTo(device, endX, endY);																					// Проводит линию до конца
}																												// Завершает отрисовку линии
void label(HDC device, int x, int y, const wchar_t* value) {													// Выводит подпись текущим шрифтом
    TextOutW(device, x, y, value, static_cast<int>(std::wcslen(value)));										// Передаёт подпись системной функции
}																												// Завершает отрисовку подписи
}																												// Завершает область вспомогательных средств графика
void drawCooling(HDC device, const RECT& area, const CoolingResult& result, int diameter) {						// Рисует историю охлаждения бочки
    if (result.samples.empty()) {																				// Проверяет наличие результата расчёта
        return;																									// Оставляет место графика свободным
    }																											// Завершает проверку результата
    const RECT plot{area.left + 69, area.top + 78, area.right - 33, area.bottom - 70};							// Задаёт область температурных кривых
    const auto& last = result.samples.back();																	// Берёт последнее рассчитанное состояние
    const auto x = [&](double seconds) {																		// Преобразует секунды в положение по горизонтали
        return plot.left + static_cast<int>(std::lround((plot.right - plot.left) * seconds / last.seconds));	// Масштабирует весь интервал времени
    };																											// Завершает преобразование времени
    const auto y = [&](double temperature) {																	// Преобразует температуру в вертикальное положение
        return plot.bottom - static_cast<int>(std::lround((plot.bottom - plot.top) * temperature / 1600));		// Располагает большие температуры сверху
    };																											// Завершает преобразование температуры
    const int saved = SaveDC(device);																			// Сохраняет настройки холста перед графиком
    const HPEN grid = CreatePen(PS_SOLID, 1, RGB(222, 229, 234));												// Создаёт светлые линии сетки
    const HPEN frame = CreatePen(PS_SOLID, 1, RGB(76, 90, 102));												// Создаёт видимые оси координат
    const HPEN axis = CreatePen(PS_SOLID, 2, axisColor);														// Создаёт линию температуры на оси
    const HPEN surface = CreatePen(PS_SOLID, 2, surfaceColor);													// Создаёт линию температуры на стенке
    SetBkMode(device, TRANSPARENT);																				// Оставляет белое поле графика за подписями
    SetTextColor(device, RGB(34, 52, 64));																		// Задаёт цвет текста и численных делений
    SetTextAlign(device, TA_LEFT | TA_TOP);																		// Размещает текст по левому верхнему углу
    wchar_t caption[120]{};																						// Резервирует строку с диаметром бочки
    std::swprintf(caption, 120, L"Диаметр бочки %d мм · воздух 20 °C", diameter);								// Указывает геометрию и температуру воздуха
    label(device, area.left + 17, area.top + 18, caption);														// Выводит параметры показанного расчёта
    SelectObject(device, axis);																					// Выбирает цвет осевой кривой для легенды
    line(device, area.left + 18, area.top + 47, area.left + 39, area.top + 47);									// Рисует образец осевой линии
    label(device, area.left + 46, area.top + 40, L"На оси");													// Подписывает осевую кривую
    SelectObject(device, surface);																				// Выбирает цвет поверхностной кривой
    line(device, area.left + 108, area.top + 47, area.left + 129, area.top + 47);								// Рисует образец поверхностной линии
    label(device, area.left + 136, area.top + 40, L"У стенки");													// Подписывает поверхностную кривую
    SelectObject(device, grid);																					// Выбирает осветлённые линии делений
    for (int temperature = 0; temperature <= 1600; temperature += 400) {										// Размечает температуру шагом четыреста градусов
        const int row = y(temperature);																			// Определяет положение горизонтальной линии
        line(device, plot.left, row, plot.right, row);															// Рисует линию температурного деления
        wchar_t number[12]{};																					// Резервирует место для численной подписи
        std::swprintf(number, 12, L"%d", temperature);															// Записывает целое число градусов
        SetTextAlign(device, TA_RIGHT | TA_TOP);																// Выравнивает подпись справа от начала
        label(device, plot.left - 7, row - 7, number);															// Выводит число слева от шкалы
    }																											// Завершает разметку температуры
    SetTextAlign(device, TA_CENTER | TA_TOP);																	// Центрирует часы под вертикальными делениями
    for (int tick = 0; tick <= 4; ++tick) {																		// Делит интервал расчёта на четыре части
        const double seconds = last.seconds * tick / 4;															// Вычисляет момент для текущего деления
        const int column = x(seconds);																			// Находит положение вертикальной линии
        line(device, column, plot.top, column, plot.bottom);													// Рисует линию временного деления
        wchar_t number[12]{};																					// Резервирует место для числа часов
        std::swprintf(number, 12, L"%.0f", seconds / 3600);														// Преобразует секунды в часы
        label(device, column, plot.bottom + 8, number);															// Выводит отметку времени под графиком
    }																											// Завершает разметку времени
    SelectObject(device, frame);																				// Выбирает тёмные линии осей
    line(device, plot.left, plot.top, plot.left, plot.bottom);													// Рисует вертикальную ось температуры
    line(device, plot.left, plot.bottom, plot.right, plot.bottom);												// Рисует горизонтальную ось времени
    SetTextAlign(device, TA_LEFT | TA_TOP);																		// Возвращает выравнивание по началу текста
    label(device, plot.left, plot.top - 20, L"Температура, °C");												// Подписывает единицу измерения температуры
    SetTextAlign(device, TA_CENTER | TA_TOP);																	// Выравнивает название горизонтальной оси
    label(device, (plot.left + plot.right) / 2, plot.bottom + 26, L"Время, ч");									// Подписывает единицу измерения времени
    for (const bool onAxis : {true, false}) {																	// Последовательно рисует обе температурные кривые
        SelectObject(device, onAxis ? axis : surface);															// Выбирает цвет точки измерения
        const auto& start = result.samples.front();																// Берёт начальный отсчёт кривой
        MoveToEx(device, x(start.seconds), y(start.axisTemperature), nullptr);									// Начинает с общей начальной температуры
        for (const auto& sample : result.samples) {																// Обходит рассчитанные моменты времени
            const double value = onAxis ? sample.axisTemperature : sample.surfaceTemperature;					// Выбирает температуру текущей кривой
            LineTo(device, x(sample.seconds), y(value));														// Добавляет отрезок до очередного отсчёта
        }																										// Завершает соединение отсчётов температуры
    }																											// Завершает построение температурных кривых
    SetTextAlign(device, TA_LEFT | TA_TOP);																		// Размещает итоговый текст вдоль нижнего края
    std::swprintf(caption, 120, L"Через %.0f ч: на оси %.1f °C, у стенки %.1f °C",								// Формирует сводку последнего отсчёта
                  last.seconds / 3600, last.axisTemperature, last.surfaceTemperature);							// Передаёт время и обе температуры
    label(device, area.left + 17, area.bottom - 22, caption);													// Показывает численные результаты под графиком
    RestoreDC(device, saved);																					// Восстанавливает исходные настройки рисования
    DeleteObject(surface);																						// Освобождает перо поверхностной кривой
    DeleteObject(axis);																							// Освобождает перо осевой кривой
    DeleteObject(frame);																						// Освобождает перо осей координат
    DeleteObject(grid);																							// Освобождает перо сетки графика
}																												// Завершает рисование температурного графика
