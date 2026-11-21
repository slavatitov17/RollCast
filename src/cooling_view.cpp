#include "cooling_view.h"	// Подключает пустой и рассчитанный рабочий экран
#include <algorithm>	// Находит экстремумы и ограничивает выбор
#include <cmath>	// Округляет деления и экранные координаты
#include <cwchar>	// Форматирует числовые подписи графика
namespace {	// Скрывает вспомогательные функции рисования
void label(HDC dc, int x, int y, const wchar_t* value) {	// Выводит одну подпись графика
    TextOutW(dc, x, y, value, static_cast<int>(std::wcslen(value)));	// Передаёт строку графической системе Windows
}	// Завершает вывод подписи
void line(HDC dc, int x1, int y1, int x2, int y2) {	// Рисует один вспомогательный отрезок
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);	// Соединяет две заданные экранные точки
}	// Завершает вспомогательную линию
void timeText(wchar_t* value, std::size_t size, double seconds) {	// Форматирует часы минуты и секунды
    const auto t = static_cast<long long>(std::llround(seconds));	// Округляет момент до целой секунды
    std::swprintf(value, size, L"%02lld:%02lld:%02lld", t / 3600, t / 60 % 60, t % 60);	// Сохраняет часы без суточного переполнения
}	// Завершает оформление времени
std::vector<POINT> curvePoints(const CoolingResult& h, bool axis, const RECT& plot, double duration, double minimum, double maximum) {	// Подготавливает линию по экранному разрешению
    std::vector<POINT> points;	// Хранит компактное представление полной истории
    const auto x = [&](std::size_t i) { return plot.left + static_cast<int>((plot.right - plot.left) * h.samples[i].seconds / duration); };	// Переводит секунды в координату времени
    const auto value = [&](std::size_t i) { return axis ? h.samples[i].axisTemperature : h.samples[i].surfaceTemperature; };	// Выбирает ось или стенку металла
    for (std::size_t first = 0; first < h.samples.size();) {	// Объединяет отсчёты одного экранного столбца
        std::size_t last = first + 1, low = first, high = first;	// Начинает поиск границ и экстремумов
        while (last < h.samples.size() && x(last) == x(first)) { ++last; }	// Находит конец группы совпадающих координат
        for (std::size_t i = first + 1; i < last; ++i) {	// Сохраняет короткие температурные пики
            if (value(i) < value(low)) { low = i; }	// Уточняет минимум группы
            if (value(i) > value(high)) { high = i; }	// Уточняет максимум группы
        }	// Завершает поиск экстремумов
        for (const auto i : {std::min(low, high), std::max(low, high), last - 1}) {	// Сохраняет исходную хронологию экстремумов
            points.push_back({x(i), plot.bottom - static_cast<LONG>(std::lround((plot.bottom - plot.top) * (value(i) - minimum) / (maximum - minimum)))});	// Добавляет масштабированную точку линии
        }	// Завершает запись экранного столбца
        first = last;	// Продолжает со следующего столбца
    }	// Завершает подготовку полной кривой
    return points;	// Возвращает линию для единственного вызова GDI
}	// Завершает кэширование температурной истории
void colorKey(HDC dc, int x, int y, COLORREF color, const wchar_t* name) {	// Рисует цветной квадрат и подпись
    const HBRUSH brush = CreateSolidBrush(color); const RECT square{x, y + 2, x + 11, y + 13};	// Готовит компактный образец цвета
    FillRect(dc, &square, brush); DeleteObject(brush);	// Рисует квадрат и освобождает кисть
    label(dc, x + 17, y, name);	// Отделяет название от цветного образца
}	// Завершает элемент легенды температур
}	// Завершает внутренние графические функции
std::array<RECT, sectionCount> chartAreas(const RECT& area, int selection) {	// Выделяет область единственного выбранного графика
    std::array<RECT, sectionCount> cards{};	// Начинает со скрытых отделений
    cards[static_cast<std::size_t>(std::clamp(selection, 0, 4))] = {area.left + 8, area.top + 260, area.right - 8, area.bottom - 8};	// Занимает доступную нижнюю часть панели
    return cards;	// Возвращает размещение выбранного отделения
}	// Завершает компоновку графика
void drawSingleChart(HDC dc, const RECT& area, const Project& inputs, const Run* run, const Run* baseline, std::size_t section, std::size_t frame, CurveCache& cache, double filling) {	// Рисует оси и доступную температурную историю
    const int saved = SaveDC(dc);	// Сохраняет настройки вызывающего контекста
    IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);	// Ограничивает график собственной карточкой
    SetBkMode(dc, TRANSPARENT); SetTextAlign(dc, TA_LEFT | TA_TOP);	// Настраивает читаемые подписи
    const Project& p = run ? run->project : inputs;	// Связывает численные результаты с принятыми входами
    const auto* result = run ? &run->cooling.section(section) : nullptr;	// Разрешает пустой график до первого расчёта
    const auto* base = baseline ? &baseline->cooling.section(section) : nullptr;	// Подключает базовую историю при её наличии
    const double duration = std::max(p.hours, baseline ? baseline->project.hours : p.hours) * 3600;	// Объединяет горизонты текущего и базового расчётов
    const double maximum = std::ceil(std::max({1600.0, p.metalTemperature, p.formTemperature, p.airTemperature, baseline ? baseline->project.metalTemperature : 0}) / 200) * 200;	// Выбирает температурный верх шкалы
    const double minimum = std::floor(std::min({0.0, p.airTemperature, p.formTemperature, baseline ? baseline->project.airTemperature : 0}) / 200) * 200;	// Учитывает возможные отрицательные температуры
    const RECT plot{area.left + 54, area.top + 75, area.right - 28, area.bottom - 94};	// Оставляет место подписям и итоговым показателям
    const auto x = [&](double t) { return plot.left + static_cast<int>((plot.right - plot.left) * t / duration); };	// Переводит время в координату графика
    const auto y = [&](double t) { return plot.bottom - static_cast<int>((plot.bottom - plot.top) * (t - minimum) / (maximum - minimum)); };	// Переводит температуру в экранную координату
    FrameRect(dc, &area, GetSysColorBrush(COLOR_3DSHADOW)); label(dc, area.left + 10, area.top + 10, sectionNames[section]);	// Подписывает выбранное отделение
    label(dc, plot.left, area.top + 52, L"Температура, °C");	// Указывает величину и единицы вертикальной оси
    wchar_t text[200]{};	// Резервирует динамические числовые подписи
    if (result) {	// Показывает температуры только при наличии результата
        const auto& sample = result->samples.at(frame);	// Выбирает общий момент расчётной истории
        std::swprintf(text, std::size(text), L"Ось %.1f °C · стенка %.1f °C", sample.axisTemperature, sample.surfaceTemperature);	// Форматирует две измеряемые температуры
        label(dc, area.left + 10, area.top + 31, text);	// Выводит рассчитанные температуры выбранного момента
    }	// Завершает заголовок рассчитанного состояния
    const HPEN grid = CreatePen(PS_DOT, 1, RGB(181, 189, 196));	// Создаёт основную пунктирную координатную сетку
    const HPEN minor = CreatePen(PS_DOT, 1, RGB(221, 226, 230));	// Создаёт более светлые промежуточные деления
    const HPEN axis = CreatePen(PS_SOLID, 2, RGB(23, 92, 150));	// Создаёт синюю линию температуры на оси
    const HPEN wall = CreatePen(PS_SOLID, 2, RGB(170, 75, 27));	// Создаёт коричневую линию температуры стенки
    const HPEN guide = CreatePen(PS_DOT, 1, RGB(90, 113, 130));	// Создаёт указатель общего момента времени
    const HPEN basePen = CreatePen(PS_DASH, 1, RGB(145, 145, 145));	// Создаёт серые линии закреплённой базы
    const double hours = duration / 3600;	// Переводит горизонт шкалы в часы
    const double tickHours = hours > 4 ? std::max(2.0, std::ceil(hours / 18) * 2) : hours > 1 ? 0.5 : hours / 4;	// Выбирает читаемый шаг примерно девяти делений
    for (double t = 0; t < hours + tickHours * 0.01; t += tickHours / 2) {	// Рисует основные и промежуточные временные деления
        const bool major = std::abs(t / tickHours - std::round(t / tickHours)) < 1e-8;	// Отличает подписанное деление от промежуточного
        SelectObject(dc, major ? grid : minor); line(dc, x(t * 3600), plot.top, x(t * 3600), plot.bottom);	// Проводит соответствующую вертикальную линию
        if (major) {	// Подписывает основные значения времени
            std::swprintf(text, std::size(text), L"%g", t); SetTextAlign(dc, TA_CENTER | TA_TOP);	// Оформляет отметку без лишних нулей
            label(dc, x(t * 3600), plot.bottom + 5, text);	// Выводит время под координатной осью
        }	// Завершает подпись основного деления
    }	// Завершает временную координатную сетку
    if (std::abs(hours / tickHours - std::round(hours / tickHours)) > 1e-6) {	// Подписывает неполный конечный интервал горизонта
        std::swprintf(text, std::size(text), L"%g", hours); SetTextAlign(dc, TA_CENTER | TA_TOP);	// Форматирует конечную длительность
        label(dc, plot.right, plot.bottom + 5, text);	// Сохраняет видимым точный конец шкалы
    }	// Завершает последнюю временную отметку
    const double tickTemperature = std::max(200.0, std::ceil((maximum - minimum) / 2000) * 200);	// Выбирает достаточное число температурных делений
    for (double t = minimum; t <= maximum; t += tickTemperature / 2) {	// Рисует подробную температурную сетку
        const bool major = std::abs((t - minimum) / tickTemperature - std::round((t - minimum) / tickTemperature)) < 1e-8;	// Выбирает подписанные температурные отметки
        SelectObject(dc, major ? grid : minor); line(dc, plot.left, y(t), plot.right, y(t));	// Проводит горизонтальное деление сетки
        if (major) { std::swprintf(text, std::size(text), L"%.0f", t); SetTextAlign(dc, TA_RIGHT | TA_TOP); label(dc, plot.left - 6, y(t) - 7, text); }	// Подписывает температуру слева от графика
    }	// Завершает температурную координатную сетку
    SetTextAlign(dc, TA_CENTER | TA_TOP); label(dc, (plot.left + plot.right) / 2, plot.bottom + 24, L"Время после заполнения, ч");	// Указывает начало отсчёта горизонтальной оси
    if (result) {	// Рисует численные линии только после расчёта
        const auto& sample = result->samples.at(frame);	// Получает кадр для указателя и показателей
        const int clipping = SaveDC(dc); IntersectClipRect(dc, plot.left, plot.top, plot.right + 1, plot.bottom + 1);	// Удерживает вспомогательные линии внутри осей
        SelectObject(dc, guide);	// Выбирает пунктир фазовых границ
        line(dc, plot.left, y(p.phase.solidus), plot.right, y(p.phase.solidus));	// Отмечает температуру солидуса металла
        line(dc, plot.left, y(p.phase.liquidus), plot.right, y(p.phase.liquidus));	// Отмечает температуру ликвидуса металла
        line(dc, x(sample.seconds), plot.top, x(sample.seconds), plot.bottom);	// Показывает выбранный момент охлаждения
        if (cache.source != result || cache.baseline != base || !EqualRect(&cache.plot, &plot)) {	// Перестраивает кривые только при изменении источника
            cache.source = result; cache.baseline = base; cache.plot = plot;	// Запоминает соответствие линий и истории
            cache.curves[0] = curvePoints(*result, true, plot, duration, minimum, maximum);	// Подготавливает текущую осевую температуру
            cache.curves[1] = curvePoints(*result, false, plot, duration, minimum, maximum);	// Подготавливает текущую температуру стенки
            cache.curves[2] = base ? curvePoints(*base, true, plot, duration, minimum, maximum) : std::vector<POINT>{};	// Подготавливает осевую линию базы
            cache.curves[3] = base ? curvePoints(*base, false, plot, duration, minimum, maximum) : std::vector<POINT>{};	// Подготавливает пристеночную линию базы
        }	// Завершает обновление кэшированных линий
        for (std::size_t i = 0; i < 4; ++i) {	// Рисует текущие и доступные базовые кривые
            SelectObject(dc, i == 0 ? axis : i == 1 ? wall : basePen);	// Выбирает цвет и тип температурной линии
            if (cache.curves[i].size() > 1) { Polyline(dc, cache.curves[i].data(), static_cast<int>(cache.curves[i].size())); }	// Выводит всю подготовленную кривую одним вызовом
        }	// Завершает рисование температурных историй
        RestoreDC(dc, clipping); SetTextAlign(dc, TA_LEFT | TA_TOP);	// Возвращает область текстовых подписей
        auto volumes = phaseVolumes(p.dimensions[section], sample);	// Учитывает обе фазы внутри двухфазной зоны
        const double filled = filling < 1 ? filledSections(p, filling)[section] : 1;	// Согласует объёмы с наглядной заливкой
        std::swprintf(text, std::size(text), L"Фазы: твёрдая %.5f · жидкая %.5f м³", volumes.solid * filled, volumes.liquid * filled);	// Форматирует интегральные фазовые объёмы
        label(dc, area.left + 10, area.bottom - 58, text);	// Показывает объёмы фаз отдельно от зон
        std::swprintf(text, std::size(text), L"Толщины зон: %.1f · %.1f · %.1f мм", sample.phase.zones[0].thickness * 1000, sample.phase.zones[1].thickness * 1000, sample.phase.zones[2].thickness * 1000);	// Указывает твёрдую двухфазную и жидкую толщины
        if (filling < 1) { std::swprintf(text, std::size(text), L"Заполнение отделения %.0f %%", filled * 100); }	// Исключает толщины для ещё неполного цилиндра
        label(dc, area.left + 10, area.bottom - 39, text);	// Выводит радиальное распределение трёх зон
        if (base && base->completedAt && result->completedAt) {	// Сравнивает только достигнутые времена затвердевания
            std::swprintf(text, std::size(text), L"Изменение времени относительно базы: %+.0f с", *result->completedAt - *base->completedAt);	// Вычисляет разность текущего и базового времени
            label(dc, area.left + 10, area.bottom - 20, text);	// Выводит доступное сравнение времени
        }	// Завершает подпись базового сравнения
    }	// Завершает отображение рассчитанных значений
    RestoreDC(dc, saved);	// Восстанавливает исходный контекст рисования
    for (const auto pen : {grid, minor, axis, wall, guide, basePen}) { DeleteObject(pen); }	// Освобождает временные графические перья
}	// Завершает пустой или рассчитанный график
void drawMoldDashboard(HDC dc, const RECT& area, const Project& inputs, const Run* run, const Run* baseline, std::size_t frame, double filling, int selection, ChartCache& cache) {	// Объединяет пустую либо рассчитанную сводку
    const int saved = SaveDC(dc); IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);	// Ограничивает результаты правой панелью
    SetBkMode(dc, TRANSPARENT); SetTextAlign(dc, TA_LEFT | TA_TOP);	// Настраивает обычное выравнивание подписей
    const int left = area.left + 12;	// Задаёт левое поле таблицы
    const int nameWidth = 145;	// Оставляет место названиям двух нижних участков
    const int column = (area.right - left - 8 - nameWidth) / 4;	// Распределяет остальные числовые колонки
    colorKey(dc, area.right - 200, area.top + 12, RGB(23, 92, 150), L"Ось");	// Обозначает синюю температурную линию
    colorKey(dc, area.right - 118, area.top + 12, RGB(170, 75, 27), L"Стенка");	// Обозначает коричневую температурную линию
    wchar_t value[180]{}, time[40]{};	// Резервирует подписи показателей
    const Project& p = run ? run->project : inputs;	// Привязывает объёмы к принятой геометрии
    if (run) {	// Показывает время только рассчитанного проекта
        if (filling < 1) { std::swprintf(value, std::size(value), L"Заливка %.0f %%", filling * 100); }	// Отмечает наглядную стадию заполнения
        else { timeText(time, std::size(time), run->cooling.section(0).samples.at(frame).seconds); std::swprintf(value, std::size(value), L"После заполнения %ls", time); }	// Форматирует общий момент охлаждения
        label(dc, left, area.top + 12, value);	// Выводит выбранный момент без стартовых чисел
    }	// Завершает подпись рассчитанного времени
    const wchar_t* headings[]{L"Твёрдая, м³", L"Двухфазная, м³", L"Жидкая, м³", L"Затвердевание"};	// Подписывает три зоны и время завершения
    label(dc, left, area.top + 42, L"Отделение");	// Называет первую колонку сводки
    for (int i = 0; i < 4; ++i) { label(dc, left + nameWidth + i * column, area.top + 42, headings[i]); }	// Размещает заголовки численных показателей
    const auto filled = filledSections(p, filling); std::array<double, 3> totals{};	// Готовит заполнение и суммы геометрических зон
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Показывает пять отделений в порядке схемы
        const int y = area.top + 67 + static_cast<int>(i) * 23;	// Располагает очередную строку таблицы
        label(dc, left, y, sectionNames[i]);	// Сохраняет имена даже при пустых результатах
        if (!run) { continue; }	// Оставляет все численные ячейки пустыми
        const auto& history = run->cooling.section(i); const auto& sample = history.samples.at(frame);	// Выбирает согласованный временной кадр
        const double volume = cylinderVolume(p.dimensions[i]);	// Получает полную вместимость отделения
        for (int zone = 0; zone < 3; ++zone) {	// Выводит непересекающиеся геометрические объёмы
            const double v = filling < 1 ? (zone == 2 ? volume * filled[i] : 0) : volume * sample.phase.zones[zone].volumeShare;	// Учитывает заполнение и температурный профиль
            totals[zone] += v; std::swprintf(value, std::size(value), L"%.4f", v);	// Накапливает и форматирует объём зоны
            label(dc, left + nameWidth + zone * column, y, value);	// Показывает объём в своей колонке
        }	// Завершает три объёма отделения
        timeText(time, std::size(time), history.completedAt.value_or(0));	// Форматирует достигнутый момент солидуса
        label(dc, left + nameWidth + 3 * column, y, history.completedAt ? time : L"Не достигнуто");	// Отличает отсутствие события от нулевого времени
    }	// Завершает строки пяти отделений
    label(dc, left, area.top + 191, L"Всего в форме");	// Обозначает сумму объёмов валка
    if (run) {	// Показывает суммы только после расчёта
        for (int zone = 0; zone < 3; ++zone) { std::swprintf(value, std::size(value), L"%.4f", totals[zone]); label(dc, left + nameWidth + zone * column, area.top + 191, value); }	// Суммирует геометрические зоны без литников
        timeText(time, std::size(time), run->cooling.completedAt().value_or(0));	// Определяет последнее затвердевающее отделение
        label(dc, left + nameWidth + 3 * column, area.top + 191, run->cooling.completedAt() ? time : L"Не достигнуто");	// Показывает завершение всего валка
    }	// Завершает итоговые численные показатели
    const auto cards = chartAreas(area, selection);	// Выбирает одну видимую карточку графика
    drawSingleChart(dc, cards[selection], p, run, baseline, selection, frame, cache[selection], filling);	// Рисует выбранное отделение даже без результата
    RestoreDC(dc, saved);	// Возвращает исходное графическое состояние
}	// Завершает панель кристаллизации металла
