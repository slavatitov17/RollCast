#include "cooling_view.h"																																	// Подключает отображение результатов и кэш линий
#include <algorithm>																																		// Обрабатывает экстремумы температурных кривых
#include <cmath>																																			// Округляет экранные координаты
#include <cwchar>																																			// Форматирует русские подписи
namespace {																																					// Ограничивает видимость функций рисования
void label(HDC dc, int x, int y, const wchar_t* text) {																										// Выводит подпись выбранным шрифтом
    TextOutW(dc, x, y, text, static_cast<int>(std::wcslen(text)));																							// Передаёт подпись графической системе Windows
}																																							// Завершает вывод подписи
void line(HDC dc, int x, int y, int endX, int endY) {																										// Проводит один служебный отрезок графика
    MoveToEx(dc, x, y, nullptr);																															// Задаёт начало отрезка
    LineTo(dc, endX, endY);																																	// Рисует отрезок выбранным пером
}																																							// Завершает построение отрезка
void timeText(wchar_t* out, std::size_t length, double seconds) {																							// Форматирует время в часы минуты секунды
    const auto value = static_cast<long long>(std::llround(seconds));																						// Округляет время до секунды
    std::swprintf(out, length, L"%02lld:%02lld:%02lld", value / 3600, value / 60 % 60, value % 60);															// Сохраняет часы без суточного переполнения
}																																			// Завершает форматирование времени
std::vector<POINT> curvePoints(const CoolingResult& history, bool axis, const RECT& plot, double endTime, double minimumTemperature, double maximum) {		// Подготавливает компактную кривую по экранным столбцам
    std::vector<POINT> points;																																// Хранит координаты готовой линии
    const auto x = [&](std::size_t i) { return plot.left + static_cast<int>((plot.right - plot.left) * history.samples[i].seconds / endTime); };			// Переводит время отсчёта в экранный столбец
    const auto value = [&](std::size_t i) { return axis ? history.samples[i].axisTemperature : history.samples[i].surfaceTemperature; };					// Выбирает одну температурную историю
    std::size_t start = 0;																																	// Начинает с первого отсчёта
    while (start < history.samples.size()) {																												// Обрабатывает отсчёты одного экранного столбца
        std::size_t end = start + 1;																														// Начинает поиск конца группы
        while (end < history.samples.size() && x(end) == x(start)) { ++end; }																				// Объединяет совпадающие экранные координаты
        std::size_t minimum = start;																														// Начинает поиск минимума группы
        std::size_t maximumIndex = start;																													// Начинает поиск максимума группы
        for (std::size_t i = start + 1; i < end; ++i) {																										// Сохраняет короткие пики температурной истории
            if (value(i) < value(minimum)) { minimum = i; }																									// Уточняет минимальный отсчёт
            if (value(i) > value(maximumIndex)) { maximumIndex = i; }																						// Уточняет максимальный отсчёт
        }																																					// Завершает поиск экстремумов группы
        for (const auto i : {std::min(minimum, maximumIndex), std::max(minimum, maximumIndex), end - 1}) {													// Сохраняет порядок экстремумов и последнюю точку
            const auto y = plot.bottom - static_cast<LONG>(std::lround((plot.bottom - plot.top) * (value(i) - minimumTemperature) / (maximum - minimumTemperature)));	// Масштабирует температуру выбранного отсчёта
            points.push_back({x(i), y});																													// Добавляет точку компактной линии
        }																																					// Завершает запись группы
        start = end;																																		// Переходит к следующему экранному столбцу
    }																																						// Завершает подготовку всей истории
    return points;																																			// Возвращает линию для единственного вызова GDI
}																																							// Завершает подготовку температурной кривой
}																																							// Завершает внутренние функции графиков
std::array<RECT, 4> chartAreas(const RECT& area, int selection) {																							// Определяет области одного или четырёх графиков
    std::array<RECT, 4> result{};																															// Начинает со скрытых графиков
    const int top = area.top + 222;																															// Оставляет место таблице и списку
    if (selection < 4) {																																	// Обрабатывает выбор отдельного отделения
        result[3 - selection] = {area.left + 6, top, area.right - 6, area.bottom - 6};																		// Занимает доступную область одним графиком
        return result;																																		// Возвращает единственный видимый график
    }																																						// Завершает выбор одиночного режима
    const int middle = (area.left + area.right) / 2;																										// Делит графики на два столбца
    const int half = (top + area.bottom) / 2;																												// Делит графики на два ряда
    result[3] = {area.left + 6, top, middle - 3, half - 3};																									// Размещает прибыль сверху слева
    result[2] = {middle + 3, top, area.right - 6, half - 3};																								// Размещает верхнюю шейку сверху справа
    result[1] = {area.left + 6, half + 3, middle - 3, area.bottom - 6};																						// Размещает бочку снизу слева
    result[0] = {middle + 3, half + 3, area.right - 6, area.bottom - 6};																					// Размещает нижнюю шейку снизу справа
    return result;																																			// Возвращает компоновку четырёх отделений
}																																							// Завершает размещение графиков
void drawSingleChart(HDC dc, const RECT& area, const Run& run, const Run* baseline,																			// Получает текущие и базовые результаты
    std::size_t section, std::size_t frame, CurveCache& cache, double filling) {																			// Выбирает отделение и общий кадр
    const int saved = SaveDC(dc);																															// Сохраняет состояние контекста
    IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);																					// Ограничивает рисование своей карточкой
    SetBkMode(dc, TRANSPARENT);																																// Сохраняет фон под текстом
    SetTextAlign(dc, TA_LEFT | TA_TOP);																														// Выравнивает подписи слева
    const auto& result = run.cooling.section(section);																										// Получает историю отделения
    const auto& sample = result.samples.at(frame);																											// Выбирает рассчитанный кадр
    const auto* base = baseline ? &baseline->cooling.section(section) : nullptr;																			// Подключает базовую историю
    const double endTime = base ? std::max(result.samples.back().seconds, base->samples.back().seconds) : result.samples.back().seconds;					// Объединяет временные диапазоны
    const double maximum = std::ceil(std::max({1600.0, run.project.metalTemperature, run.project.formTemperature, run.project.airTemperature, baseline ? baseline->project.metalTemperature : 0, baseline ? baseline->project.formTemperature : 0, baseline ? baseline->project.airTemperature : 0}) / 200) * 200;	// Выбирает общий температурный масштаб
    const double minimumTemperature = std::floor(std::min({0.0, run.project.metalTemperature, run.project.formTemperature, run.project.airTemperature, baseline ? baseline->project.airTemperature : 0, baseline ? baseline->project.formTemperature : 0}) / 200) * 200;	// Учитывает отрицательные температуры исходных условий
    const RECT plot{area.left + 40, area.top + 58, area.right - 15, area.bottom - 94};																		// Оставляет место под динамические подписи
    const auto x = [&](double t) { return plot.left + static_cast<int>((plot.right - plot.left) * t / endTime); };											// Переводит время в координату графика
    const auto y = [&](double t) { return plot.bottom - static_cast<int>((plot.bottom - plot.top) * (t - minimumTemperature) / (maximum - minimumTemperature)); };	// Переводит температуру в координату графика
    FrameRect(dc, &area, GetSysColorBrush(COLOR_3DSHADOW));																									// Обводит границу графика
    label(dc, area.left + 9, area.top + 7, sectionNames[section]);																							// Показывает название отделения
    wchar_t text[180]{};																																	// Резервирует подписи текущих величин
    std::swprintf(text, std::size(text), L"Ось %.1f °C · стенка %.1f °C", sample.axisTemperature, sample.surfaceTemperature);								// Форматирует температуры выбранного момента
    label(dc, area.left + 9, area.top + 26, text);																											// Показывает значения температур
    const HPEN grid = CreatePen(PS_SOLID, 1, RGB(224, 230, 235));																			// Создаёт перо координатной сетки
    const HPEN axis = CreatePen(PS_SOLID, 2, RGB(23, 92, 150));																								// Создаёт перо осевой температуры
    const HPEN wall = CreatePen(PS_SOLID, 2, RGB(170, 75, 27));																								// Создаёт перо температуры стенки
    const HPEN guide = CreatePen(PS_DOT, 1, RGB(125, 140, 151));																							// Создаёт перо фазовых границ
    const HPEN basePen = CreatePen(PS_DOT, 1, RGB(145, 145, 145));																							// Создаёт перо базового расчёта
    SelectObject(dc, grid);																																	// Выбирает оформление сетки
    for (int tick = 0; tick <= 2; ++tick) {																													// Подписывает три основных деления шкал
        const double temperature = minimumTemperature + (maximum - minimumTemperature) * tick / 2;															// Определяет температуру деления
        line(dc, plot.left, y(temperature), plot.right, y(temperature));																					// Проводит горизонтальную линию сетки
        std::swprintf(text, std::size(text), L"%.0f", temperature);																							// Форматирует температурную отметку
        SetTextAlign(dc, TA_RIGHT | TA_TOP);																												// Выравнивает отметки слева от графика
        label(dc, plot.left - 4, y(temperature) - 7, text);																									// Подписывает температуру
        const double seconds = endTime * tick / 2;																											// Определяет время деления
        line(dc, x(seconds), plot.top, x(seconds), plot.bottom);																							// Проводит вертикальную линию сетки
        std::swprintf(text, std::size(text), L"%.1f", seconds / 3600);																						// Переводит время в часы
        SetTextAlign(dc, TA_CENTER | TA_TOP);																												// Центрирует временные отметки
        label(dc, x(seconds), plot.bottom + 3, text);																										// Подписывает время
    }																																						// Завершает координатную сетку
    label(dc, (plot.left + plot.right) / 2, plot.bottom + 18, L"Время, ч");																					// Называет временную ось
    SelectObject(dc, guide);																																// Выбирает пунктир вспомогательных линий
    line(dc, plot.left, y(run.project.phase.solidus), plot.right, y(run.project.phase.solidus));															// Отмечает солидус текущего сплава
    line(dc, plot.left, y(run.project.phase.liquidus), plot.right, y(run.project.phase.liquidus));															// Отмечает ликвидус текущего сплава
    line(dc, x(sample.seconds), plot.top, x(sample.seconds), plot.bottom);																					// Отмечает общий выбранный момент
    if (cache.source != &result || cache.baseline != base || !EqualRect(&cache.plot, &plot)) {																// Подготавливает линии только после изменения масштаба
        cache.source = &result;																																// Запоминает источник текущей истории
        cache.baseline = base;																																// Запоминает источник базы сравнения
        cache.plot = plot;																																	// Запоминает область масштабирования
        cache.curves[0] = curvePoints(result, true, plot, endTime, minimumTemperature, maximum);															// Подготавливает осевую кривую
        cache.curves[1] = curvePoints(result, false, plot, endTime, minimumTemperature, maximum);															// Подготавливает пристеночную кривую
        cache.curves[2] = base ? curvePoints(*base, true, plot, endTime, minimumTemperature, maximum) : std::vector<POINT>{};								// Подготавливает осевую базовую кривую
        cache.curves[3] = base ? curvePoints(*base, false, plot, endTime, minimumTemperature, maximum) : std::vector<POINT>{};								// Подготавливает пристеночную базовую кривую
    }																																						// Завершает обновление кэша
    for (std::size_t i = 0; i < 4; ++i) {																													// Рисует доступные подготовленные линии
        SelectObject(dc, i == 0 ? axis : i == 1 ? wall : basePen);																							// Выбирает оформление текущей линии
        const auto& points = cache.curves[i];																												// Получает компактный набор точек
        if (points.size() > 1) { Polyline(dc, points.data(), static_cast<int>(points.size())); }															// Рисует всю кривую одним вызовом GDI
    }																																						// Завершает вывод температурных историй
    SetTextAlign(dc, TA_LEFT | TA_TOP);																														// Возвращает выравнивание динамических подписей
    auto phases = phaseVolumes(run.project.dimensions[section], sample);																					// Вычисляет фазовые объёмы отделения
    const double filled = filling < 1 ? filledSections(run.project, filling)[section] : 1;																	// Учитывает ещё неполное заполнение отделения
    phases.solid *= filled; phases.liquid *= filled;																										// Согласует фазовые объёмы со стадией заливки
    std::swprintf(text, std::size(text), L"Фазы: тв. %.5f · жидк. %.5f м³", phases.solid, phases.liquid);													// Форматирует объёмы с двухфазной составляющей
    label(dc, area.left + 9, area.bottom - 56, text);																										// Показывает интегральные фазовые объёмы
    std::swprintf(text, std::size(text), L"Толщины: %.1f · %.1f · %.1f мм", sample.phase.zones[0].thickness * 1000, sample.phase.zones[1].thickness * 1000, sample.phase.zones[2].thickness * 1000);	// Показывает толщины трёх геометрических зон
    if (filling < 1) { std::swprintf(text, std::size(text), L"Заполнение отделения %.0f %%", filled * 100); }												// Заменяет радиальные толщины при неполной заливке
    label(dc, area.left + 9, area.bottom - 38, text);																										// Соблюдает порядок твёрдая двухфазная жидкая
    if (base && base->completedAt && result.completedAt) {																									// Проверяет возможность сравнения полного затвердевания
        std::swprintf(text, std::size(text), L"База: Δt %+.0f с · U %.4f м/с", *result.completedAt - *base->completedAt, sample.circulationSpeed);			// Показывает изменение времени относительно базы
    } else {																																				// Обрабатывает отсутствие базового момента
        std::swprintf(text, std::size(text), L"Масштаб циркуляции %.4f м/с", sample.circulationSpeed);														// Показывает масштаб скорости текущего кадра
    }																																						// Завершает подпись сравнения
    label(dc, area.left + 9, area.bottom - 20, text);																										// Выводит последнюю строку графика
    RestoreDC(dc, saved);																																	// Восстанавливает исходный контекст рисования
    for (const HPEN pen : {grid, axis, wall, guide, basePen}) { DeleteObject(pen); }																		// Освобождает созданные графические ресурсы
}																																							// Завершает рисование одного графика
void drawMoldDashboard(HDC dc, const RECT& area, const Run& run, const Run* baseline,																		// Получает результат и закреплённую базу
    std::size_t frame, double filling, int selection, ChartCache& cache) {																					// Принимает общий момент и выбор графиков
    const int saved = SaveDC(dc);																															// Сохраняет графическое состояние
    IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);																					// Ограничивает результаты своей панелью
    SetBkMode(dc, TRANSPARENT);																																// Сохраняет фон текста
    SetTextAlign(dc, TA_LEFT | TA_TOP);																														// Выравнивает строки таблицы
    const int left = area.left + 12;																														// Задаёт отступ таблицы
    const int column = (area.right - left - 12 - 105) / 4;																									// Распределяет ширину числовых столбцов
    const auto filled = filledSections(run.project, filling);																								// Получает фактически заполненные объёмы
    wchar_t text[180]{};																																	// Резервирует строку показателей
    wchar_t time[30]{};																																		// Резервирует строку времени
    if (filling < 1) { std::swprintf(text, std::size(text), L"Заливка %.0f %%", filling * 100); }															// Показывает стадию заполнения
    else { timeText(time, std::size(time), run.cooling.section(0).samples[frame].seconds); std::swprintf(text, std::size(text), L"После заполнения %ls", time); }	// Показывает расчётное время охлаждения
    label(dc, left, area.top + 10, text);																													// Выводит текущую стадию
    label(dc, left + 260, area.top + 10, baseline ? L"Серый пунктир – база" : L"Синяя – ось · коричневая – стенка");										// Поясняет линии графиков
    const int header = area.top + 40;																														// Задаёт строку заголовков
    label(dc, left, header, L"Отделение");																													// Называет первую колонку
    const wchar_t* headings[]{L"Твёрдая, м³", L"Двухфазная, м³", L"Жидкая, м³", L"Затвердевание"};															// Называет геометрические зоны и момент завершения
    for (int i = 0; i < 4; ++i) { label(dc, left + 105 + i * column, header, headings[i]); }																// Подписывает числовые столбцы
    std::array<double, 3> totals{};																															// Суммирует непересекающиеся геометрические объёмы
    for (std::size_t row = 0; row < 4; ++row) {																												// Показывает отделения сверху вниз
        const std::size_t section = 3 - row;																												// Выбирает отделение модели
        const auto& history = run.cooling.section(section);																									// Получает его температурную историю
        const auto& sample = history.samples[frame];																										// Выбирает общий кадр
        const double volume = cylinderVolume(run.project.dimensions[section]);																				// Находит полный геометрический объём
        const int y = header + 24 + static_cast<int>(row) * 21;																								// Размещает очередную строку
        label(dc, left, y, sectionNames[section]);																											// Подписывает отделение
        for (int zone = 0; zone < 3; ++zone) {																												// Показывает три геометрические зоны
            const double value = filling < 1 ? (zone == 2 ? volume * filled[section] : 0) : volume * sample.phase.zones[zone].volumeShare;					// Учитывает фазовое состояние и заполнение
            totals[zone] += value;																															// Добавляет объём к итогу формы
            std::swprintf(text, std::size(text), L"%.5f", value);																							// Форматирует объём зоны
            label(dc, left + 105 + zone * column, y, text);																									// Выводит объём в своей колонке
        }																																					// Завершает три геометрических объёма
        timeText(time, std::size(time), history.completedAt.value_or(0));																					// Форматирует время затвердевания отделения
        label(dc, left + 105 + 3 * column, y, history.completedAt ? time : L"Не достигнуто");																// Показывает доступный момент завершения
    }																																						// Завершает строки отделений
    label(dc, left, header + 112, L"Всего в форме");																										// Подписывает сумму трёх зон
    for (int zone = 0; zone < 3; ++zone) {																													// Выводит итоговые геометрические объёмы
        std::swprintf(text, std::size(text), L"%.5f", totals[zone]);																						// Форматирует сумму по форме
        label(dc, left + 105 + zone * column, header + 112, text);																							// Выводит сумму зоны
    }																																						// Завершает итоговую строку
    timeText(time, std::size(time), run.cooling.completedAt().value_or(0));																					// Форматирует завершение кристаллизации всей формы
    label(dc, left + 105 + 3 * column, header + 112, run.cooling.completedAt() ? time : L"Не достигнуто");													// Показывает общий момент завершения
    const auto charts = chartAreas(area, selection);																										// Размещает выбранные графики
    for (std::size_t section = 0; section < 4; ++section) {																									// Рисует видимые отделения
        if (!IsRectEmpty(&charts[section])) { drawSingleChart(dc, charts[section], run, baseline, section, frame, cache[section], filling); }				// Использует кэшированные температурные линии
    }																																						// Завершает построение графиков
    RestoreDC(dc, saved);																																	// Восстанавливает состояние перед рисованием
}																																							// Завершает панель кристаллизации
