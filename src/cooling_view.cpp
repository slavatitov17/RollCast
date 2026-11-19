#include "cooling_view.h"																													// Подключает интерфейс температурного графика
#include <cmath>																															// Округляет координаты линий и текста
#include <cwchar>																															// Форматирует время и температуры для вывода
namespace {																																	// Оставляет отрисовку подписей внутри модуля графика
constexpr COLORREF axisColor = RGB(23, 92, 150);																							// Задаёт синий цвет осевой кривой
constexpr COLORREF surfaceColor = RGB(170, 75, 27);																							// Задаёт тёплый цвет поверхностной кривой
void line(HDC device, int x, int y, int endX, int endY) {																					// Рисует линию текущим пером
    MoveToEx(device, x, y, nullptr);																										// Ставит перо в начало линии
    LineTo(device, endX, endY);																												// Проводит линию до конца
}																																			// Завершает отрисовку линии
void label(HDC device, int x, int y, const wchar_t* value) {																				// Выводит подпись текущим шрифтом
    TextOutW(device, x, y, value, static_cast<int>(std::wcslen(value)));																	// Передаёт подпись системной функции
}																																			// Завершает отрисовку подписи
void timeText(wchar_t* output, std::size_t length, double seconds) {																		// Форматирует часы минуты и секунды расчёта
    const auto rounded = static_cast<int>(std::lround(seconds));																			// Округляет момент до ближайшей секунды
    std::swprintf(output, length, L"%02d:%02d:%02d", rounded / 3600, rounded / 60 % 60, rounded % 60);										// Записывает время с ведущими нулями
}																																			// Завершает форматирование времени
void drawSectionChart(HDC device, const RECT& area, const CoolingResult& result,															// Рисует один из четырёх графиков
    const SectionDimensions& dimensions, const wchar_t* name, std::size_t frameIndex) {														// Принимает отделение и общий момент
    const int saved = SaveDC(device);																										// Сохраняет состояние рисования
    IntersectClipRect(device, area.left, area.top, area.right, area.bottom);																// Ограничивает рисунок текущей карточкой
    FrameRect(device, &area, GetSysColorBrush(COLOR_3DSHADOW));																				// Обводит границы графика отделения
    const auto& sample = result.samples[frameIndex];																						// Выбирает состояние общего момента
    const auto& last = result.samples.back();																								// Читает конец временной истории
    const RECT plot{area.left + 38, area.top + 58, area.right - 16, area.bottom - 122};														// Оставляет место под фазовые данные
    const auto x = [&](double seconds) {																									// Переводит время в координату графика
        return plot.left + static_cast<int>(std::lround((plot.right - plot.left) * seconds / last.seconds));								// Масштабирует общую временную шкалу
    };																																		// Завершает преобразование времени
    const auto y = [&](double temperature) {																								// Переводит температуру в экранную координату
        return plot.bottom - static_cast<int>(std::lround((plot.bottom - plot.top) * temperature / 1600));									// Сохраняет одинаковый масштаб температур
    };																																		// Завершает преобразование температуры
    SetTextAlign(device, TA_LEFT | TA_TOP);																									// Выравнивает подписи относительно левого края
    label(device, area.left + 9, area.top + 7, name);																						// Называет отделение над графиком
    wchar_t text[160]{};																													// Резервирует текст текущих значений
    std::swprintf(text, std::size(text), L"Ось %.1f °C · стенка %.1f °C", sample.axisTemperature, sample.surfaceTemperature);				// Форматирует температуры выбранного кадра
    label(device, area.left + 9, area.top + 25, text);																						// Выводит значения обеих кривых
    const HPEN grid = CreatePen(PS_SOLID, 1, RGB(224, 230, 235));																			// Создаёт перо координатной сетки
    const HPEN axis = CreatePen(PS_SOLID, 2, axisColor);																					// Создаёт перо температуры на оси
    const HPEN surface = CreatePen(PS_SOLID, 2, surfaceColor);																				// Создаёт перо температуры у стенки
    const HPEN guide = CreatePen(PS_DOT, 1, RGB(125, 140, 151));																			// Создаёт перо выбранного момента
    SelectObject(device, grid);																												// Выбирает тонкие линии сетки
    for (int temperature = 0; temperature <= 1600; temperature += 800) {																	// Подписывает общую температурную шкалу
        const int row = y(temperature);																										// Находит положение температурной отметки
        line(device, plot.left, row, plot.right, row);																						// Проводит горизонтальную линию сетки
        std::swprintf(text, std::size(text), L"%d", temperature);																			// Форматирует температуру деления
        SetTextAlign(device, TA_RIGHT | TA_TOP);																							// Выравнивает отметку слева от графика
        label(device, plot.left - 4, row - 7, text);																						// Подписывает температуру в градусах
    }																																		// Завершает построение температурных делений
    SetTextAlign(device, TA_CENTER | TA_TOP);																								// Выравнивает временные отметки по центру
    for (int tick = 0; tick <= 2; ++tick) {																									// Показывает начало середину и конец истории
        const double seconds = last.seconds * tick / 2;																						// Вычисляет время деления
        line(device, x(seconds), plot.top, x(seconds), plot.bottom);																		// Проводит вертикальную линию сетки
        std::swprintf(text, std::size(text), L"%.0f", seconds / 3600);																		// Переводит секунды в часы
        label(device, x(seconds), plot.bottom + 4, text);																					// Подписывает время в часах
    }																																		// Завершает временную шкалу
    label(device, (plot.left + plot.right) / 2, plot.bottom + 20, L"Время, ч");																// Указывает величину и единицу времени
    SelectObject(device, guide);																											// Выбирает пунктир для фазовых границ
    line(device, plot.left, y(demoPhase.solidus), plot.right, y(demoPhase.solidus));														// Отмечает температуру полного затвердевания
    line(device, plot.left, y(demoPhase.liquidus), plot.right, y(demoPhase.liquidus));														// Отмечает температуру начала затвердевания
    line(device, x(sample.seconds), plot.top, x(sample.seconds), plot.bottom);																// Отмечает общий выбранный момент
    for (const bool onAxis : {true, false}) {																								// Выводит обе температурные истории
        SelectObject(device, onAxis ? axis : surface);																						// Выбирает цвет кривой
        MoveToEx(device, x(0), y(result.samples.front().axisTemperature), nullptr);															// Начинает кривую с исходного состояния
        for (const auto& frame : result.samples) {																							// Обходит всю историю охлаждения
            LineTo(device, x(frame.seconds), y(onAxis ? frame.axisTemperature : frame.surfaceTemperature));									// Добавляет следующий отрезок кривой
        }																																	// Завершает построение одной кривой
    }																																		// Завершает построение температурных графиков
    SetTextAlign(device, TA_LEFT | TA_TOP);																									// Возвращает выравнивание фазовых подписей
    std::swprintf(text, std::size(text), L"Масштаб циркуляции %.4f м/с", sample.circulationSpeed);											// Показывает расчётный масштаб движения
    label(device, area.left + 9, area.bottom - 83, text);																					// Выводит скорость под графиком
    constexpr const wchar_t* zoneNames[]{L"Твёрдая", L"Двухфазная", L"Жидкая"};																// Называет геометрические зоны
    const double volume = cylinderVolume(dimensions);																						// Находит полный объём отделения
    for (std::size_t i = 0; i < 3; ++i) {																									// Выводит характеристики каждой зоны
        const auto& zone = sample.phase.zones[i];																							// Читает толщину и долю зоны
        std::swprintf(text, std::size(text), L"%ls  %.1f мм · %.5f м³", zoneNames[i], zone.thickness * 1000, zone.volumeShare * volume);	// Форматирует толщину и геометрический объём
        label(device, area.left + 9, area.bottom - 62 + static_cast<int>(i) * 18, text);													// Размещает строку фазовой зоны
    }																																		// Завершает таблицу геометрических зон
    RestoreDC(device, saved);																												// Восстанавливает исходные параметры рисования
    DeleteObject(grid);																														// Освобождает перо сетки
    DeleteObject(axis);																														// Освобождает перо осевой температуры
    DeleteObject(surface);																													// Освобождает перо температуры стенки
    DeleteObject(guide);																													// Освобождает перо выбранного момента
}																																			// Завершает график одного отделения
void drawMoldOverview(HDC device, const RECT& area, const MoldCooling& cooling,																// Рисует сводную таблицу текущих фазовых объёмов
    const MoldDimensions& dimensions, std::size_t frameIndex) {																				// Получает геометрию и общий кадр расчёта
    const int left = area.left + 18;																										// Задаёт отступ от левой границы панели
    const int top = area.top + 14;																											// Задаёт отступ от верхней границы панели
    if (cooling.sampleCount() == 0) {																										// Проверяет наличие согласованных расчётных историй
        label(device, left, top, L"Для этих размеров расчёт прерван");																		// Сообщает об отсутствии рассчитанной истории
        return;																																// Пропускает вывод незаполненной таблицы
    }																																		// Завершает проверку результата
    const int solidColumn = left + 129;																										// Выбирает столбец объёма твёрдого металла
    const int liquidColumn = left + 250;																									// Выбирает столбец объёма жидкого металла
    const int completionColumn = left + 371;																								// Выбирает столбец полного затвердевания
    wchar_t time[24]{};																														// Резервирует строку общего времени
    wchar_t text[100]{};																													// Резервирует строку чисел таблицы
    timeText(time, std::size(time), cooling.section(0).samples[frameIndex].seconds);														// Читает время общего кадра
    std::swprintf(text, std::size(text), L"Момент расчёта  %ls", time);																		// Показывает время с начала охлаждения
    label(device, left, top, text);																											// Выводит общий момент над таблицей
    const HPEN axisPen = CreatePen(PS_SOLID, 2, axisColor);																					// Обозначает температуру на оси
    const HPEN wallPen = CreatePen(PS_SOLID, 2, surfaceColor);																				// Обозначает температуру у стенки
    const auto oldPen = SelectObject(device, axisPen);																						// Сохраняет перо до легенды
    line(device, left + 190, top + 7, left + 210, top + 7);																					// Рисует синий образец линии
    label(device, left + 216, top, L"На оси");																								// Подписывает первую кривую
    SelectObject(device, wallPen);																											// Выбирает цвет второй кривой
    line(device, left + 285, top + 7, left + 305, top + 7);																					// Рисует коричневый образец линии
    label(device, left + 311, top, L"У стенки");																							// Подписывает вторую кривую
    SelectObject(device, oldPen);																											// Восстанавливает исходное перо
    DeleteObject(axisPen);																													// Освобождает ресурс первой линии
    DeleteObject(wallPen);																													// Освобождает ресурс второй линии
    const int header = top + 28;																											// Определяет высоту строки заголовков
    const RECT band{left - 5, header - 5, area.right - 12, header + 19};																	// Задаёт фон заголовков таблицы
    FillRect(device, &band, GetSysColorBrush(COLOR_3DFACE));																				// Отделяет заголовки от численных результатов
    label(device, left, header, L"Отделение");																								// Подписывает столбец имён отделений
    label(device, solidColumn, header, L"Твёрдая фаза, м³");																				// Подписывает объём твёрдой фазы
    label(device, liquidColumn, header, L"Жидкая фаза, м³");																				// Подписывает объём жидкой фазы
    label(device, completionColumn, header, L"Затвердевание");																				// Подписывает столбец времени
    const auto row = [&](int y, const wchar_t* name, PhaseVolumes volumes, std::optional<double> completion) {								// Выводит строку фазовых данных
        label(device, left, y, name);																										// Выводит название отделения или строки итога
        std::swprintf(text, std::size(text), L"%.5f", volumes.solid);																		// Форматирует твёрдый объём в кубометрах
        label(device, solidColumn, y, text);																								// Выводит значение твёрдого объёма
        std::swprintf(text, std::size(text), L"%.5f", volumes.liquid);																		// Форматирует жидкий объём в кубометрах
        label(device, liquidColumn, y, text);																								// Выводит значение жидкого объёма
        if (completion) {																													// Проверяет затвердевание в пределах истории
            timeText(time, std::size(time), *completion);																					// Форматирует расчётное время затвердевания
        }																																	// Завершает обработку завершившегося затвердевания
        label(device, completionColumn, y, completion ? time : L"Позже 36 ч");																// Показывает время или выход за горизонт
    };																																		// Завершает подготовку строки таблицы
    for (std::size_t i = 0; i < dimensions.size(); ++i) {																					// Проходит отделения формы снизу вверх
        const auto section = dimensions.size() - 1 - i;																						// Выбирает отделения сверху вниз
        const auto& result = cooling.section(section);																						// Получает историю текущего отделения
        const auto volumes = phaseVolumes(dimensions[section], result.samples[frameIndex]);													// Выбирает фазовые объёмы общего момента
        row(header + 22 + static_cast<int>(i) * 20, sectionNames[section], volumes, result.completedAt);									// Показывает состояние одного отделения
    }																																		// Завершает вывод отдельных отделений формы
    const RECT rule{left - 5, header + 98, area.right - 12, header + 99};																	// Разделяет отделения и итог
    FillRect(device, &rule, GetSysColorBrush(COLOR_3DSHADOW));																				// Рисует линию перед итоговой строкой
    row(header + 104, L"Всего в форме", cooling.totalVolumes(dimensions, frameIndex), cooling.completedAt());								// Добавляет итог четырёх отделений
}																																			// Завершает построение сводной таблицы
}																																			// Завершает область внутренних функций отображения
void drawMoldDashboard(HDC device, const RECT& area, const MoldCooling& cooling,															// Рисует постоянную панель всех результатов
    const MoldDimensions& dimensions, std::size_t frameIndex) {																				// Принимает общую геометрию и кадр
    const int saved = SaveDC(device);																										// Сохраняет состояние контекста рисования
    IntersectClipRect(device, area.left + 1, area.top + 1, area.right - 1, area.bottom - 1);												// Ограничивает результаты своей панелью
    drawMoldOverview(device, area, cooling, dimensions, frameIndex);																		// Выводит суммарные и индивидуальные объёмы фаз
    if (cooling.sampleCount() > 0) {																										// Проверяет наличие рассчитанных историй
        const int top = area.top + 178;																										// Размещает графики под сводкой
        const int middle = (area.left + area.right) / 2;																					// Делит графики на два столбца
        const int half = (top + area.bottom) / 2;																							// Делит графики на два ряда
        const RECT charts[]{																												// Задаёт четыре независимые области графиков
            {area.left + 6, top, middle - 3, half - 3},																						// Размещает прибыль сверху слева
            {middle + 3, top, area.right - 6, half - 3},																					// Размещает верхнюю шейку сверху справа
            {area.left + 6, half + 3, middle - 3, area.bottom - 6},																			// Размещает бочку снизу слева
            {middle + 3, half + 3, area.right - 6, area.bottom - 6},																		// Размещает нижнюю шейку снизу справа
        };																																	// Завершает расположение графиков
        for (std::size_t i = 0; i < dimensions.size(); ++i) {																				// Показывает все отделения без переключения
            const auto section = dimensions.size() - 1 - i;																					// Сохраняет порядок сверху вниз
            drawSectionChart(device, charts[i], cooling.section(section), dimensions[section], sectionNames[section], frameIndex);			// Рисует график соответствующего отделения
        }																																	// Завершает отображение четырёх графиков
    }																																		// Завершает проверку расчётных данных
    RestoreDC(device, saved);																												// Восстанавливает исходную область рисования
}																																			// Завершает общую панель результатов
