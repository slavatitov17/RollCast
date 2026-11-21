#include "cooling_view.h"	// Описывает параметры текущего действия
#include <algorithm>	// Описывает параметры текущего действия
#include <cmath>	// Описывает параметры текущего действия
#include <cwchar>	// Описывает параметры текущего действия
namespace {	// Описывает параметры текущего действия
void label(HDC dc, int x, int y, const wchar_t* value) {	// Описывает параметры текущего действия
    TextOutW(dc, x, y, value, static_cast<int>(std::wcslen(value)));	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
void line(HDC dc, int x1, int y1, int x2, int y2) {	// Описывает параметры текущего действия
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
void timeText(wchar_t* value, std::size_t size, double seconds) {	// Описывает параметры текущего действия
    const auto t = static_cast<long long>(std::llround(seconds));	// Описывает параметры текущего действия
    std::swprintf(value, size, L"%02lld:%02lld:%02lld", t / 3600, t / 60 % 60, t % 60);	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
void colorKey(HDC dc, int x, int y, COLORREF color, const wchar_t* name) {	// Описывает параметры текущего действия
    const HBRUSH brush = CreateSolidBrush(color); const RECT square{x, y + 2, x + 10, y + 12};	// Описывает параметры текущего действия
    FillRect(dc, &square, brush); DeleteObject(brush); label(dc, x + 15, y, name);	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
constexpr COLORREF colors[]{RGB(45,112,181), RGB(242,177,46), RGB(228,76,57)};	// Согласует три цвета слоев со схемой
std::vector<POINT> curvePoints(const CoolingResult& h, int zone, double volume, const RECT& plot, double duration, double maximum) {	// Описывает параметры текущего действия
    std::vector<POINT> points;	// Описывает параметры текущего действия
    const auto x = [&](std::size_t i) { return plot.left + static_cast<int>((plot.right - plot.left) * h.samples[i].seconds / duration); };	// Описывает параметры текущего действия
    const auto value = [&](std::size_t i) { return h.samples[i].phase.zones[zone].volumeShare * volume; };	// Переводит долю геометрической зоны в объем
    for (std::size_t first = 0; first < h.samples.size();) {	// Описывает параметры текущего действия
        std::size_t last = first + 1, low = first, high = first;	// Описывает параметры текущего действия
        while (last < h.samples.size() && x(last) == x(first)) { ++last; }	// Описывает параметры текущего действия
        for (std::size_t i = first + 1; i < last; ++i) {	// Описывает параметры текущего действия
            if (value(i) < value(low)) { low = i; }	// Описывает параметры текущего действия
            if (value(i) > value(high)) { high = i; }	// Описывает параметры текущего действия
        }	// Завершает текущий блок обработки
        for (const auto i : {std::min(low, high), std::max(low, high), last - 1}) {	// Описывает параметры текущего действия
            const double ordinate = std::clamp(value(i) / maximum, -2.0, 20.0);	// Ограничивает экранные координаты без изменения физических объемов
            points.push_back({x(i), plot.bottom - static_cast<LONG>(std::lround((plot.bottom - plot.top) * ordinate))});	// Описывает параметры текущего действия
        }	// Завершает текущий блок обработки
        first = last;	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    return points;	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
std::array<RECT, sectionCount> chartAreas(const RECT& area, int selection) {	// Описывает параметры текущего действия
    std::array<RECT, sectionCount> cards{};	// Описывает параметры текущего действия
    cards[static_cast<std::size_t>(std::clamp(selection, 0, 4))] = {area.left + 8, area.top + 260, area.right - 8, area.bottom - 8};	// Описывает параметры текущего действия
    return cards;	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
void drawSingleChart(HDC dc, const RECT& area, const Project& inputs, const Run* run, const Run* baseline, std::size_t section, std::size_t frame, CurveCache& cache, double filling, int zoom) {	// Описывает параметры текущего действия
    const int saved = SaveDC(dc); IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);	// Описывает параметры текущего действия
    SetBkMode(dc, TRANSPARENT); SetTextAlign(dc, TA_LEFT | TA_TOP);	// Описывает параметры текущего действия
    const Project& p = run ? run->project : inputs;	// Описывает параметры текущего действия
    const auto* result = run ? &run->cooling.section(section) : nullptr;	// Описывает параметры текущего действия
    const auto* base = baseline ? &baseline->cooling.section(section) : nullptr;	// Описывает параметры текущего действия
    const double duration = std::max(1.0, std::max(result ? result->samples.back().seconds : p.autoTime ? 3600 : p.hours * 3600, base ? base->samples.back().seconds : 0));	// Использует фактический горизонт автоматического результата
    const double volume = cylinderVolume(p.dimensions[section]);	// Описывает параметры текущего действия
    const double maximum = std::max(volume, baseline ? cylinderVolume(baseline->project.dimensions[section]) : volume) * 100.0 / zoom;	// Масштабирует объем без перемещения графика
    const RECT plot{area.left + 72, area.top + 88, area.right - 20, area.bottom - 106};	// Описывает параметры текущего действия
    const auto x = [&](double t) { return plot.left + static_cast<int>((plot.right - plot.left) * t / duration); };	// Описывает параметры текущего действия
    const auto y = [&](double v) { return plot.bottom - static_cast<int>((plot.bottom - plot.top) * v / maximum); };	// Описывает параметры текущего действия
    FrameRect(dc, &area, GetSysColorBrush(COLOR_3DSHADOW));	// Описывает параметры текущего действия
    LOGFONTW font{}; GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(font), &font);	// Описывает параметры текущего действия
    font.lfHeight = -16; font.lfWeight = FW_BOLD; const HFONT title = CreateFontIndirectW(&font);	// Выделяет название выбранного отделения
    const auto originalFont = SelectObject(dc, title); SetTextAlign(dc, TA_CENTER | TA_TOP);	// Описывает параметры текущего действия
    label(dc, (area.left + area.right) / 2, area.top + 9, sectionNames[section]);	// Описывает параметры текущего действия
    SelectObject(dc, originalFont); DeleteObject(title);	// Описывает параметры текущего действия
    font.lfHeight = -13; font.lfWeight = FW_NORMAL; font.lfEscapement = 900; font.lfOrientation = 900;	// Поворачивает подпись оси объема вертикально
    const HFONT vertical = CreateFontIndirectW(&font); SelectObject(dc, vertical);	// Описывает параметры текущего действия
    label(dc, area.left + 11, (plot.top + plot.bottom) / 2, L"Объем, м3");	// Описывает параметры текущего действия
    SelectObject(dc, originalFont); DeleteObject(vertical);	// Описывает параметры текущего действия
    wchar_t text[256]{};	// Описывает параметры текущего действия
    const HPEN grid = CreatePen(PS_DOT, 1, RGB(185,190,195));	// Описывает параметры текущего действия
    const HPEN guide = CreatePen(PS_DOT, 1, RGB(90,113,130));	// Описывает параметры текущего действия
    SelectObject(dc, grid);	// Описывает параметры текущего действия
    const double minutes = duration / 60;	// Описывает параметры текущего действия
    const double rawTick = minutes / 8; const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(0.001,rawTick))));	// Подбирает удобный шаг минутной шкалы
    const double ratio = rawTick / magnitude; const double tick = (ratio <= 1 ? 1 : ratio <= 2 ? 2 : ratio <= 5 ? 5 : 10) * magnitude;	// Выбирает деления один два или пять
    for (double t = 0; t <= minutes + 1e-8; t += tick / 2) {	// Описывает параметры текущего действия
        line(dc, x(t * 60), plot.top, x(t * 60), plot.bottom);	// Описывает параметры текущего действия
        if (std::abs(t/tick - std::round(t/tick)) < 1e-7) { std::swprintf(text, std::size(text), L"%g", t); SetTextAlign(dc, TA_CENTER | TA_TOP); label(dc, x(t * 60), plot.bottom + 5, text); }	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    if (minutes - tick * std::floor(minutes / tick) > tick / 3) { std::swprintf(text, std::size(text), L"%g", minutes); SetTextAlign(dc, TA_CENTER | TA_TOP); label(dc, plot.right, plot.bottom + 5, text); }	// Подписывает конечный момент числового горизонта
    for (int k = 0; k <= 8; ++k) {	// Описывает параметры текущего действия
        const double value = maximum * k / 8; line(dc, plot.left, y(value), plot.right, y(value));	// Описывает параметры текущего действия
        if (k % 2 == 0) { std::swprintf(text, std::size(text), L"%.3g", value); SetTextAlign(dc, TA_RIGHT | TA_TOP); label(dc, plot.left - 6, y(value) - 7, text); }	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    SetTextAlign(dc, TA_CENTER | TA_TOP); label(dc, (plot.left + plot.right) / 2, plot.bottom + 24, L"Время, мин");	// Описывает параметры текущего действия
    if (result) {	// Описывает параметры текущего действия
        const auto& sample = result->samples.at(frame);	// Описывает параметры текущего действия
        if (cache.source != result || cache.baseline != base || !EqualRect(&cache.plot, &plot)) {	// Описывает параметры текущего действия
            cache.source = result; cache.baseline = base; cache.plot = plot;	// Описывает параметры текущего действия
            for (int z = 0; z < 3; ++z) {	// Описывает параметры текущего действия
                cache.curves[z] = curvePoints(*result, z, volume, plot, duration, maximum);	// Описывает параметры текущего действия
                cache.curves[z+3] = base ? curvePoints(*base, z, cylinderVolume(baseline->project.dimensions[section]), plot, duration, maximum) : std::vector<POINT>{};	// Описывает параметры текущего действия
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        const int clipped = SaveDC(dc); IntersectClipRect(dc, plot.left, plot.top, std::min(plot.right + 1, x(sample.seconds) + 1), plot.bottom + 1);	// Не раскрывает будущие результаты раньше выбранного момента
        for (int z = 0; z < 6; ++z) {	// Описывает параметры текущего действия
            const HPEN pen = CreatePen(z < 3 ? PS_SOLID : PS_DASH, z < 3 ? 2 : 1, z < 3 ? colors[z] : RGB(155,155,155));	// Описывает параметры текущего действия
            SelectObject(dc, pen);	// Описывает параметры текущего действия
            if (cache.curves[z].size() > 1) { Polyline(dc, cache.curves[z].data(), static_cast<int>(cache.curves[z].size())); }	// Описывает параметры текущего действия
            SelectObject(dc, grid); DeleteObject(pen);	// Описывает параметры текущего действия
        }	// Завершает текущий блок обработки
        RestoreDC(dc, clipped); SelectObject(dc, guide); line(dc, x(sample.seconds), plot.top, x(sample.seconds), plot.bottom);	// Описывает параметры текущего действия
        SetTextAlign(dc, TA_LEFT | TA_TOP);	// Описывает параметры текущего действия
        const auto filled = filledSections(p, filling)[section];	// Описывает параметры текущего действия
        double values[3]{};	// Описывает параметры текущего действия
        for (int z = 0; z < 3; ++z) { values[z] = filling < 1 ? (z == 2 ? volume * filled : 0) : volume * sample.phase.zones[z].volumeShare; }	// Описывает параметры текущего действия
        std::swprintf(text, std::size(text), L"Объем слоя, м3: Твердый %.4f, Двухфазный %.4f, Жидкий %.4f", values[0], values[1], values[2]);	// Описывает параметры текущего действия
        label(dc, area.left + 9, area.bottom - 58, text);	// Описывает параметры текущего действия
        std::swprintf(text, std::size(text), L"Толщина слоя, мм: Твердый %.1f, Двухфазный %.1f, Жидкий %.1f", sample.phase.zones[0].thickness*1000, sample.phase.zones[1].thickness*1000, sample.phase.zones[2].thickness*1000);	// Описывает параметры текущего действия
        if (filling < 1) { std::swprintf(text, std::size(text), L"Заполнение отделения %.0f %%", filled * 100); }	// Описывает параметры текущего действия
        label(dc, area.left + 9, area.bottom - 38, text);	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    SetTextAlign(dc, TA_LEFT | TA_TOP);	// Описывает параметры текущего действия
    const wchar_t* names[]{L"Твердый", L"Двухфазный", L"Жидкий"};	// Описывает параметры текущего действия
    for (int z = 0; z < 3; ++z) { colorKey(dc, area.left + 12 + z * 106, area.top + 68, colors[z], names[z]); }	// Описывает параметры текущего действия
    colorKey(dc, area.right - 175, area.bottom - 18, RGB(23,92,150), L"Ось");	// Описывает параметры текущего действия
    colorKey(dc, area.right - 95, area.bottom - 18, RGB(170,75,27), L"Стенка");	// Описывает параметры текущего действия
    RestoreDC(dc, saved); DeleteObject(grid); DeleteObject(guide);	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
void drawMoldDashboard(HDC dc, const RECT& area, const Project& inputs, const Run* run, const Run* baseline, std::size_t frame, double filling, int selection, ChartCache& cache, int zoom) {	// Описывает параметры текущего действия
    const int saved = SaveDC(dc); IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);	// Описывает параметры текущего действия
    SetBkMode(dc, TRANSPARENT); SetTextAlign(dc, TA_LEFT | TA_TOP);	// Описывает параметры текущего действия
    const int left = area.left + 9; const int right = area.right - 9;	// Описывает параметры текущего действия
    const int nameWidth = 145; const int column = (right - left - nameWidth) / 4;	// Описывает параметры текущего действия
    wchar_t value[256]{}, time[40]{};	// Описывает параметры текущего действия
    const Project& p = run ? run->project : inputs;	// Описывает параметры текущего действия
    double seconds = 0;	// Описывает параметры текущего действия
    if (run) {	// Описывает параметры текущего действия
        seconds = run->cooling.section(0).samples.at(frame).seconds;	// Описывает параметры текущего действия
        if (filling < 1) { std::swprintf(value, std::size(value), L"Заливка %.0f %%", filling * 100); }	// Описывает параметры текущего действия
        else { timeText(time, std::size(time), seconds); std::swprintf(value, std::size(value), L"Время %ls", time); }	// Описывает параметры текущего действия
        label(dc, left, area.top + 8, value);	// Описывает параметры текущего действия
        const auto& s = run->cooling.section(selection).samples.at(frame);	// Описывает параметры текущего действия
        std::swprintf(value, std::size(value), L"Ось %.1f град C, Стенка %.1f град C", s.axisTemperature, s.surfaceTemperature);	// Описывает параметры текущего действия
        SetTextAlign(dc, TA_RIGHT | TA_TOP); label(dc, right, area.top + 26, value); SetTextAlign(dc, TA_LEFT | TA_TOP);	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    const HPEN grid = CreatePen(PS_SOLID, 1, RGB(185,185,185)); const auto oldPen = SelectObject(dc, grid);	// Описывает параметры текущего действия
    const int top = area.top + 49; const int header = 35; const int row = 20;	// Описывает параметры текущего действия
    const wchar_t* headings[]{L"Отделение", L"Твердый\nслой, м3", L"Двухфазный\nслой, м3", L"Жидкий\nслой, м3", L"Время\nзатвердевания"};	// Описывает параметры текущего действия
    for (int col = 0; col < 5; ++col) {	// Описывает параметры текущего действия
        const int x1 = col == 0 ? left : left + nameWidth + (col-1)*column;	// Описывает параметры текущего действия
        const int x2 = col == 4 ? right : col == 0 ? left + nameWidth : x1 + column;	// Описывает параметры текущего действия
        line(dc, x1, top, x1, top + header + 6*row);	// Описывает параметры текущего действия
        RECT cell{x1+4,top+4,x2-2,top+header}; DrawTextW(dc, headings[col], -1, &cell, DT_LEFT | DT_WORDBREAK);	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    line(dc, right, top, right, top+header+6*row);	// Описывает параметры текущего действия
    line(dc, left, top, right, top);	// Описывает параметры текущего действия
    for (int r = 0; r <= 6; ++r) { line(dc, left, top+header+r*row, right, top+header+r*row); }	// Описывает параметры текущего действия
    std::array<double,3> totals{};	// Описывает параметры текущего действия
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Описывает параметры текущего действия
        const int y = top + header + static_cast<int>(i)*row+4; label(dc,left+4,y,sectionNames[i]);	// Описывает параметры текущего действия
        if (!run) { continue; }	// Описывает параметры текущего действия
        const auto& history = run->cooling.section(i); const auto& sample = history.samples.at(frame);	// Описывает параметры текущего действия
        const double volume = cylinderVolume(p.dimensions[i]);	// Описывает параметры текущего действия
        for (int z = 0; z < 3; ++z) {	// Описывает параметры текущего действия
            const double v = filling < 1 ? (z == 2 ? volume * filledSections(p,filling)[i] : 0) : volume * sample.phase.zones[z].volumeShare;	// Описывает параметры текущего действия
            totals[z] += v; std::swprintf(value,std::size(value),L"%.4f",v); label(dc,left+nameWidth+z*column+4,y,value);	// Описывает параметры текущего действия
        }	// Завершает текущий блок обработки
        const bool done = filling >= 1 && history.completedAt && seconds + 1e-6 >= *history.completedAt;	// Описывает параметры текущего действия
        timeText(time,std::size(time),history.completedAt.value_or(0)); label(dc,left+nameWidth+3*column+4,y, done ? time : L"Идет расчет...");	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    const int y = top + header+5*row+4; label(dc,left+4,y,L"Всего в форме");	// Описывает параметры текущего действия
    if (run) {	// Описывает параметры текущего действия
        for (int z=0;z<3;++z) { std::swprintf(value,std::size(value),L"%.4f",totals[z]);label(dc,left+nameWidth+z*column+4,y,value); }	// Описывает параметры текущего действия
        const auto completion=run->cooling.completedAt(); const bool done=filling>=1 && completion && seconds+1e-6>=*completion;	// Описывает параметры текущего действия
        timeText(time,std::size(time),completion.value_or(0));label(dc,left+nameWidth+3*column+4,y,done?time:L"Идет расчет...");	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    SelectObject(dc,oldPen);DeleteObject(grid);	// Описывает параметры текущего действия
    const auto cards=chartAreas(area,selection);	// Описывает параметры текущего действия
    drawSingleChart(dc,cards[selection],p,run,baseline,selection,frame,cache[selection],filling,zoom);	// Описывает параметры текущего действия
    RestoreDC(dc,saved);	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
