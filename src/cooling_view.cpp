#include "cooling_view.h"																										// Подключает интерфейс температурного графика
#include <cmath>																												// Округляет координаты линий и текста
#include <cwchar>																												// Форматирует время и температуры для вывода
namespace {																														// Оставляет отрисовку подписей внутри модуля графика
constexpr COLORREF axisColor = RGB(23, 92, 150);																				// Задаёт синий цвет осевой кривой
constexpr COLORREF surfaceColor = RGB(170, 75, 27);																				// Задаёт тёплый цвет поверхностной кривой
void line(HDC device, int x, int y, int endX, int endY) {																		// Рисует линию текущим пером
    MoveToEx(device, x, y, nullptr);																							// Ставит перо в начало линии
    LineTo(device, endX, endY);																									// Проводит линию до конца
}																																// Завершает отрисовку линии
void label(HDC device, int x, int y, const wchar_t* value) {																	// Выводит подпись текущим шрифтом
    TextOutW(device, x, y, value, static_cast<int>(std::wcslen(value)));														// Передаёт подпись системной функции
}																																// Завершает отрисовку подписи
void timeText(wchar_t* output, std::size_t length, double seconds) {															// Форматирует часы минуты и секунды расчёта
    const auto rounded = static_cast<int>(std::lround(seconds));																// Округляет момент до ближайшей секунды
    std::swprintf(output, length, L"%02d:%02d:%02d", rounded / 3600, rounded / 60 % 60, rounded % 60);							// Записывает время с ведущими нулями
}																																// Завершает форматирование времени
void phaseTable(HDC device, const RECT& area, const CoolingResult& result,														// Выводит фазовое состояние отделения в текущем кадре
    const SectionDimensions& dimensions, const CoolingSample& sample) {															// Принимает размеры и выбранное состояние
    const double volume = cylinderVolume(dimensions);																			// Вычисляет полный геометрический объём отделения
    const int left = area.left + 17;																							// Оставляет отступ от рамки окна
    const int top = area.bottom - 120;																							// Располагает таблицу ниже шкалы времени
    const int thicknessColumn = left + 145;																						// Размещает столбец радиальных толщин
    const int volumeColumn = left + 256;																						// Размещает столбец геометрических объёмов
    SetTextAlign(device, TA_LEFT | TA_TOP);																						// Выравнивает подписи таблицы слева
    label(device, left, top, L"Зона");																							// Подписывает столбец фазовых зон
    label(device, thicknessColumn, top, L"Толщина, мм");																		// Подписывает единицу толщины слоя
    label(device, volumeColumn, top, L"Объём зоны, м³");																		// Подписывает единицу геометрического объёма
    constexpr const wchar_t* names[]{L"Твёрдая", L"Двухфазная", L"Жидкая"};														// Перечисляет зоны в порядке расчёта
    wchar_t text[130]{};																										// Резервирует строки чисел и итогов
    for (int i = 0; i < 3; ++i) {																								// Выводит три строки фазовых зон
        const auto& zone = sample.phase.zones[i];																				// Получает толщину и долю объёма
        const int row = top + 19 + 16 * i;																						// Определяет положение строки на экране
        label(device, left, row, names[i]);																						// Показывает название фазовой зоны
        std::swprintf(text, std::size(text), L"%.1f", zone.thickness * 1000);													// Переводит радиальную толщину в миллиметры
        label(device, thicknessColumn, row, text);																				// Показывает радиальную толщину выбранной зоны
        std::swprintf(text, std::size(text), L"%.5f", zone.volumeShare * volume);												// Переводит объёмную долю в кубометры
        label(device, volumeColumn, row, text);																					// Показывает объём геометрической зоны
    }																															// Завершает вывод строк таблицы
    const auto volumes = phaseVolumes(dimensions, sample);																		// Вычисляет обе фазы текущего отделения
    std::swprintf(text, std::size(text), L"Объёмы фаз  ·  твёрдая %.5f м³  ·  жидкая %.5f м³", volumes.solid, volumes.liquid);	// Показывает фазовые объёмы с учётом смеси
    label(device, left, top + 72, text);																						// Выводит итоговые объёмы обеих фаз
    if (result.completedAt) {																									// Проверяет окончание затвердевания в пределах расчёта
        wchar_t completion[24]{};																								// Резервирует время полного затвердевания
        timeText(completion, std::size(completion), *result.completedAt);														// Форматирует время пересечения солидуса
        std::swprintf(text, std::size(text), L"Полное затвердевание  %ls", completion);											// Подписывает время полного затвердевания
        label(device, left, top + 93, text);																					// Выводит результат под таблицей
    } else {																													// Обрабатывает более длительное затвердевание
        label(device, left, top + 93, L"Полное затвердевание за пределами 36 ч");												// Указывает выход времени за горизонт расчёта
    }																															// Завершает отображение времени полного затвердевания
}																																// Завершает вывод фазовой таблицы
}																																// Завершает область вспомогательных средств графика
void drawCooling(HDC device, const RECT& area, const CoolingResult& result,														// Принимает историю охлаждения и затвердевания
    const SectionDimensions& dimensions, std::size_t frameIndex) {																// Рисует кривые и фазовые показатели
    if (result.samples.empty()) {																								// Проверяет наличие результата расчёта
        label(device, area.left + 17, area.top + 18, L"Для этих размеров расчёт прерван");										// Сообщает об отсутствии рассчитанной истории
        return;																													// Оставляет место графика свободным
    }																															// Завершает проверку результата
    const RECT plot{area.left + 69, area.top + 99, area.right - 33, area.bottom - 170};											// Задаёт область температурных кривых
    const auto& selected = result.samples[frameIndex];																			// Выбирает состояние по ползунку времени
    const auto& last = result.samples.back();																					// Берёт последнее рассчитанное состояние
    const auto x = [&](double seconds) {																						// Преобразует секунды в положение по горизонтали
        return plot.left + static_cast<int>(std::lround((plot.right - plot.left) * seconds / last.seconds));					// Масштабирует весь интервал времени
    };																															// Завершает преобразование времени
    const auto y = [&](double temperature) {																					// Преобразует температуру в вертикальное положение
        return plot.bottom - static_cast<int>(std::lround((plot.bottom - plot.top) * temperature / 1600));						// Располагает большие температуры сверху
    };																															// Завершает преобразование температуры
    const int saved = SaveDC(device);																							// Сохраняет настройки холста перед графиком
    const HPEN grid = CreatePen(PS_SOLID, 1, RGB(222, 229, 234));																// Создаёт светлые линии сетки
    const HPEN frame = CreatePen(PS_SOLID, 1, RGB(76, 90, 102));																// Создаёт видимые оси координат
    const HPEN axis = CreatePen(PS_SOLID, 2, axisColor);																		// Создаёт линию температуры на оси
    const HPEN surface = CreatePen(PS_SOLID, 2, surfaceColor);																	// Создаёт линию температуры на стенке
    const HPEN guide = CreatePen(PS_DOT, 1, RGB(136, 153, 164));																// Создаёт пунктир для порогов и кадра
    SetBkMode(device, TRANSPARENT);																								// Оставляет белое поле графика за подписями
    SetTextColor(device, RGB(34, 52, 64));																						// Задаёт цвет текста и численных делений
    SetTextAlign(device, TA_LEFT | TA_TOP);																						// Размещает текст по левому верхнему углу
    wchar_t caption[120]{};																										// Резервирует строку с диаметром отделения
    std::swprintf(caption, 120, L"Диаметр %d мм · солидус 1320 °C · ликвидус 1450 °C", dimensions.diameter);					// Указывает геометрию и температуру воздуха
    label(device, area.left + 17, area.top + 18, caption);																		// Выводит параметры показанного расчёта
    SelectObject(device, axis);																									// Выбирает цвет осевой кривой для легенды
    line(device, area.left + 18, area.top + 47, area.left + 39, area.top + 47);													// Рисует образец осевой линии
    label(device, area.left + 46, area.top + 40, L"На оси");																	// Подписывает осевую кривую
    SelectObject(device, surface);																								// Выбирает цвет поверхностной кривой
    line(device, area.left + 108, area.top + 47, area.left + 129, area.top + 47);												// Рисует образец поверхностной линии
    label(device, area.left + 136, area.top + 40, L"У стенки");																	// Подписывает поверхностную кривую
    wchar_t selectedTime[24]{};																									// Резервирует строку выбранного времени
    timeText(selectedTime, std::size(selectedTime), selected.seconds);															// Форматирует время выбранного отсчёта
    std::swprintf(caption, 120, L"%ls   ·   Ось %.1f °C   ·   Стенка %.1f °C",													// Готовит сводку для выбранного момента
        selectedTime, selected.axisTemperature, selected.surfaceTemperature);													// Передаёт время и температуры металла
    label(device, area.left + 17, area.top + 61, caption);																		// Отображает значения над температурными кривыми
    std::swprintf(caption, 120, L"Масштаб циркуляции %.4f м/с", selected.circulationSpeed);										// Подписывает скорость выбранного кадра
    label(device, area.left + 240, area.top + 40, caption);																		// Выводит масштаб учебного движения
    SelectObject(device, grid);																									// Выбирает осветлённые линии делений
    for (int temperature = 0; temperature <= 1600; temperature += 400) {														// Размечает температуру шагом четыреста градусов
        const int row = y(temperature);																							// Определяет положение горизонтальной линии
        line(device, plot.left, row, plot.right, row);																			// Рисует линию температурного деления
        wchar_t number[12]{};																									// Резервирует место для численной подписи
        std::swprintf(number, 12, L"%d", temperature);																			// Записывает целое число градусов
        SetTextAlign(device, TA_RIGHT | TA_TOP);																				// Выравнивает подпись справа от начала
        label(device, plot.left - 7, row - 7, number);																			// Выводит число слева от шкалы
    }																															// Завершает разметку температуры
    SetTextAlign(device, TA_CENTER | TA_TOP);																					// Центрирует часы под вертикальными делениями
    for (int tick = 0; tick <= 4; ++tick) {																						// Делит интервал расчёта на четыре части
        const double seconds = last.seconds * tick / 4;																			// Вычисляет момент для текущего деления
        const int column = x(seconds);																							// Находит положение вертикальной линии
        line(device, column, plot.top, column, plot.bottom);																	// Рисует линию временного деления
        wchar_t number[12]{};																									// Резервирует место для числа часов
        std::swprintf(number, 12, L"%.0f", seconds / 3600);																		// Преобразует секунды в часы
        label(device, column, plot.bottom + 8, number);																			// Выводит отметку времени под графиком
    }																															// Завершает разметку времени
    SelectObject(device, frame);																								// Выбирает тёмные линии осей
    line(device, plot.left, plot.top, plot.left, plot.bottom);																	// Рисует вертикальную ось температуры
    line(device, plot.left, plot.bottom, plot.right, plot.bottom);																// Рисует горизонтальную ось времени
    SetTextAlign(device, TA_LEFT | TA_TOP);																						// Возвращает выравнивание по началу текста
    label(device, plot.left, plot.top - 20, L"Температура, °C");																// Подписывает единицу измерения температуры
    SetTextAlign(device, TA_CENTER | TA_TOP);																					// Выравнивает название горизонтальной оси
    label(device, (plot.left + plot.right) / 2, plot.bottom + 26, L"Время, ч");													// Подписывает единицу измерения времени
    SelectObject(device, guide);																								// Выбирает пунктирные линии фазовых порогов
    line(device, plot.left, y(demoPhase.solidus), plot.right, y(demoPhase.solidus));											// Отмечает температуру солидуса
    line(device, plot.left, y(demoPhase.liquidus), plot.right, y(demoPhase.liquidus));											// Отмечает температуру ликвидуса
    line(device, x(selected.seconds), plot.top, x(selected.seconds), plot.bottom);												// Показывает выбранный момент времени
    for (const bool onAxis : {true, false}) {																					// Последовательно рисует обе температурные кривые
        SelectObject(device, onAxis ? axis : surface);																			// Выбирает цвет точки измерения
        const auto& start = result.samples.front();																				// Берёт начальный отсчёт кривой
        MoveToEx(device, x(start.seconds), y(start.axisTemperature), nullptr);													// Начинает с общей начальной температуры
        for (const auto& sample : result.samples) {																				// Обходит рассчитанные моменты времени
            const double value = onAxis ? sample.axisTemperature : sample.surfaceTemperature;									// Выбирает температуру текущей кривой
            LineTo(device, x(sample.seconds), y(value));																		// Добавляет отрезок до очередного отсчёта
        }																														// Завершает соединение отсчётов температуры
    }																															// Завершает построение температурных кривых
    phaseTable(device, area, result, dimensions, selected);																		// Выводит данные выбранного кадра под графиком
    RestoreDC(device, saved);																									// Восстанавливает исходные настройки рисования
    DeleteObject(guide);																										// Освобождает перо пунктирных ориентиров
    DeleteObject(surface);																										// Освобождает перо поверхностной кривой
    DeleteObject(axis);																											// Освобождает перо осевой кривой
    DeleteObject(frame);																										// Освобождает перо осей координат
    DeleteObject(grid);																											// Освобождает перо сетки графика
}																																// Завершает рисование температурного графика
void drawMoldOverview(HDC device, const RECT& area, const MoldCooling& cooling,													// Рисует сводную таблицу текущих фазовых объёмов
    const MoldDimensions& dimensions, std::size_t frameIndex) {																	// Получает геометрию и общий кадр расчёта
    const int left = area.left + 18;																							// Задаёт отступ от левой границы панели
    const int top = area.top + 21;																								// Задаёт отступ от верхней границы панели
    if (cooling.sampleCount() == 0) {																							// Проверяет наличие согласованных расчётных историй
        label(device, left, top, L"Для этих размеров расчёт прерван");															// Сообщает об отсутствии рассчитанной истории
        return;																													// Пропускает вывод незаполненной таблицы
    }																															// Завершает проверку результата
    const int solidColumn = left + 129;																							// Выбирает столбец объёма твёрдого металла
    const int liquidColumn = left + 250;																						// Выбирает столбец объёма жидкого металла
    const int completionColumn = left + 371;																					// Выбирает столбец полного затвердевания
    wchar_t time[24]{};																											// Резервирует строку общего времени
    wchar_t text[100]{};																										// Резервирует строку чисел таблицы
    timeText(time, std::size(time), cooling.section(0).samples[frameIndex].seconds);											// Читает время общего кадра
    std::swprintf(text, std::size(text), L"Момент расчёта  %ls", time);															// Показывает время с начала охлаждения
    label(device, left, top, text);																								// Выводит общий момент над таблицей
    const int header = top + 37;																								// Определяет высоту строки заголовков
    const RECT band{left - 5, header - 5, area.right - 12, header + 19};														// Задаёт фон заголовков таблицы
    FillRect(device, &band, GetSysColorBrush(COLOR_3DFACE));																	// Отделяет заголовки от численных результатов
    label(device, left, header, L"Отделение");																					// Подписывает столбец имён отделений
    label(device, solidColumn, header, L"Твёрдая фаза, м³");																	// Подписывает объём твёрдой фазы
    label(device, liquidColumn, header, L"Жидкая фаза, м³");																	// Подписывает объём жидкой фазы
    label(device, completionColumn, header, L"Затвердевание");																	// Подписывает столбец времени
    const auto row = [&](int y, const wchar_t* name, PhaseVolumes volumes, std::optional<double> completion) {					// Выводит строку фазовых данных
        label(device, left, y, name);																							// Выводит название отделения или строки итога
        std::swprintf(text, std::size(text), L"%.5f", volumes.solid);															// Форматирует твёрдый объём в кубометрах
        label(device, solidColumn, y, text);																					// Выводит значение твёрдого объёма
        std::swprintf(text, std::size(text), L"%.5f", volumes.liquid);															// Форматирует жидкий объём в кубометрах
        label(device, liquidColumn, y, text);																					// Выводит значение жидкого объёма
        if (completion) {																										// Проверяет затвердевание в пределах истории
            timeText(time, std::size(time), *completion);																		// Форматирует расчётное время затвердевания
        }																														// Завершает обработку завершившегося затвердевания
        label(device, completionColumn, y, completion ? time : L"Позже 36 ч");													// Показывает время или выход за горизонт
    };																															// Завершает подготовку строки таблицы
    for (std::size_t i = 0; i < dimensions.size(); ++i) {																		// Проходит отделения формы снизу вверх
        const auto& result = cooling.section(i);																				// Получает историю текущего отделения
        const auto volumes = phaseVolumes(dimensions[i], result.samples[frameIndex]);											// Выбирает фазовые объёмы общего момента
        row(header + 24 + static_cast<int>(i) * 24, sectionNames[i], volumes, result.completedAt);								// Показывает состояние одного отделения
    }																															// Завершает вывод отдельных отделений формы
    const RECT rule{left - 5, header + 115, area.right - 12, header + 116};														// Разделяет отделения и итог
    FillRect(device, &rule, GetSysColorBrush(COLOR_3DSHADOW));																	// Рисует линию перед итоговой строкой
    row(header + 123, L"Всего в форме", cooling.totalVolumes(dimensions, frameIndex), cooling.completedAt());					// Добавляет итог четырёх отделений
}																																// Завершает построение сводной таблицы
