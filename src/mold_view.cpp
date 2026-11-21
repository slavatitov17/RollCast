#include "mold_view.h"	// Подключает размеры и интерфейс отрисовки
#include <algorithm>	// Предоставляет поиск крайних размеров и заполнение массива
#include <cmath>	// Предоставляет округление координат до пикселей
#include <initializer_list>	// Позволяет задавать контуры списками точек
#include <cwchar>	// Измеряет длину подписи для Win32
#include <vector>	// Хранит экранные точки многоугольника
MoldHitMap::~MoldHitMap() {	// Освобождает карту перед завершением программы
    clear();	// Удаляет оставшиеся графические области
}	// Завершает текущий блок обработки
void MoldHitMap::clear() {	// Удаляет экранные области старого кадра
    for (const auto& area : areas) {	// Обходит записанные элементы формы
        DeleteObject(area.region);	// Освобождает графический дескриптор
    }	// Завершает текущий блок обработки
    areas.clear();	// Удаляет освобожденные элементы карты
    if (casting) { DeleteObject(casting); casting = nullptr; }	// Освобождает объединенную полость отливки
    if (runners) { DeleteObject(runners); runners = nullptr; }	// Освобождает объединенные каналы
}	// Завершает текущий блок обработки
void MoldHitMap::add(HRGN region, MoldPart part) {	// Сохраняет область вместе с именем элемента
    if (region) {	// Проверяет успешность создания области
        areas.push_back({region, part});	// Сохраняет область в порядке отрисовки
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
void MoldHitMap::addFluid(HRGN region, bool metal) {	// Объединяет белые полости без стенок формы
    HRGN& target = metal ? casting : runners;	// Выбирает отливку или литниковую систему
    if (!target) { target = CreateRectRgn(0, 0, 0, 0); }	// Создает начальную пустую область
    CombineRgn(target, target, region, RGN_OR);	// Добавляет очередную полость к маске
}	// Завершает текущий блок обработки
MoldPart MoldHitMap::partAt(POINT cursor) const {	// Находит видимый элемент под курсором
    for (auto it = areas.rbegin(); it != areas.rend(); ++it) {	// Ищет от верхнего слоя изображения
        if (PtInRegion(it->region, cursor.x, cursor.y)) {	// Проверяет попадание в контур элемента
            return it->part;	// Возвращает первый видимый элемент
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    return MoldPart::none;	// Оставляет форму без подсветки вне контуров
}	// Завершает текущий блок обработки
namespace {	// Оставляет вспомогательные средства внутри модуля разреза
struct Location {	// Хранит координаты точки на чертеже
    double x;	// Задает положение поперек формы
    double y;	// Задает положение от верхней части формы вниз
};	// Завершает объявление структуры или набора данных
struct Drawing {	// Преобразует точки чертежа в координаты окна
    HDC device;	// Принимает контекст отрисовки окна
    double scale;	// Определяет единый масштаб по обеим осям
    double originX;	// Задает экранную координату оси валка
    double originY;	// Задает экранную координату верха прибыли
    const RECT& viewport;	// Ограничивает подпись рамкой чертежа
    MoldHitMap& hits;	// Принимает карту экранных областей формы
    MoldPart hovered;	// Хранит текущий элемент под курсором
    HPEN outline;	// Задает обычный контур детали
    HPEN mutedOutline;	// Задает осветленный контур остальных деталей
    HBRUSH mutedFill;	// Хранит кисть приглушенных элементов
    bool rebuild;	// Управляет обновлением геометрической карты
    void style(MoldPart part, HBRUSH fill) const {	// Выбирает яркость текущей части формы
        const bool muted = hovered != MoldPart::none && hovered != part;	// Проверяет необходимость приглушения элемента
        SelectObject(device, muted ? mutedOutline : outline);	// Выбирает цвет контура
        SelectObject(device, muted ? mutedFill : fill);	// Выбирает обычную или осветленную заливку
    }	// Завершает текущий блок обработки
    POINT pixel(Location point) const {	// Переводит одну точку в пиксели
        return {std::lround(originX + point.x * scale),	// Округляет горизонтальную координату
                std::lround(originY + point.y * scale)};	// Округляет вертикальную координату
    }	// Завершает текущий блок обработки
    void shape(std::initializer_list<Location> outlinePoints, HBRUSH fill, MoldPart part) const {	// Рисует и регистрирует замкнутый контур
        std::vector<POINT> points;	// Подготавливает точки для GDI
        points.reserve(outlinePoints.size());	// Резервирует память для всего контура
        for (const Location vertex : outlinePoints) {	// Обходит опорные точки контура
            points.push_back(pixel(vertex));	// Добавляет экранную точку
        }	// Завершает текущий блок обработки
        const int count = static_cast<int>(points.size());	// Переводит размер к типу Win32
        style(part, fill);	// Устанавливает цвет элемента формы
        Polygon(device, points.data(), count);	// Рисует заполненный контур
        if (rebuild) {	// Строит области только после изменения геометрии
            const HRGN region = CreatePolygonRgn(points.data(), count, ALTERNATE);	// Создает область многоугольника
            if (fill == GetStockObject(WHITE_BRUSH)) {	// Распознает внутреннюю полость формы
                const bool metal = part == MoldPart::profit || part == MoldPart::upperModel || part == MoldPart::plug || part == MoldPart::barrel || part == MoldPart::lowerModelMiddle || part == MoldPart::lowerModelBottom;	// Отделяет отливку от литников
                hits.addFluid(region, metal);	// Дополняет маску металла
            }	// Завершает текущий блок обработки
            hits.add(region, part);	// Передает области владение геометрической карте
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    void box(double left, double top, double right, double bottom, HBRUSH fill, MoldPart part) const {	// Рисует прямоугольный элемент
        shape({{left, top}, {right, top}, {right, bottom}, {left, bottom}}, fill, part);	// Передает четыре угла элемента
    }	// Завершает текущий блок обработки
    void moveTo(Location point) const {	// Устанавливает начальную точку линии
        const POINT screen = pixel(point);	// Вычисляет экранные координаты конца
        MoveToEx(device, screen.x, screen.y, nullptr);	// Перемещает перо
    }	// Завершает текущий блок обработки
    void lineTo(Location point) const {	// Проводит отрезок до заданной точки
        const POINT screen = pixel(point);	// Вычисляет экранные координаты конца
        LineTo(device, screen.x, screen.y);	// Рисует отрезок текущим пером
    }	// Завершает текущий блок обработки
    void curveTo(Location first, Location second, Location end) const {	// Проводит кубическую кривую Безье
        const POINT points[]{pixel(first), pixel(second), pixel(end)};	// Преобразует две опорные и конечную точки
        PolyBezierTo(device, points, 3);	// Рисует кривую от текущего положения пера
    }	// Завершает текущий блок обработки
};	// Завершает объявление структуры или набора данных
HBRUSH createStipple(HDC device) {	// Создает мелкий точечный узор подложки
    BITMAPINFO texture{};	// Подготавливает параметры восьмипиксельного образца
    texture.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);	// Задает размер заголовка формата DIB
    texture.bmiHeader.biWidth = 8;	// Задает ширину повторяемого образца
    texture.bmiHeader.biHeight = -8;	// Задает строки образца сверху вниз
    texture.bmiHeader.biPlanes = 1;	// Указывает одну цветовую плоскость
    texture.bmiHeader.biBitCount = 32;	// Хранит по четыре байта на пиксель
    void* data = nullptr;	// Принимает адрес буфера пикселей
    const HBITMAP bitmap = CreateDIBSection(device, &texture, DIB_RGB_COLORS, &data, nullptr, 0);	// Создает образец кисти
    if (!bitmap) {	// Проверяет успешность создания образца
        return CreateSolidBrush(RGB(231, 237, 241));	// Возвращает однотонную подложку при ошибке
    }	// Завершает текущий блок обработки
    auto* pixels = static_cast<DWORD*>(data);	// Получает доступ к каждому пикселю
    std::fill_n(pixels, 64, 0xE7EDF1);	// Заполняет образец светлым цветом
    pixels[9] = pixels[45] = 0x8597A4;	// Расставляет две точки по диагонали
    const HBRUSH brush = CreatePatternBrush(bitmap);	// Создает кисть с повторяемым узором
    DeleteObject(bitmap);	// Освобождает исходный образец после создания кисти
    return brush;	// Передает кисть вызывающей функции
}	// Завершает текущий блок обработки
void drawFasteners(const Drawing& drawing, double radius, double height, HBRUSH brush, MoldPart part) {	// Рисует парные скобы опоки
    for (double side : {-1.0, 1.0}) {	// Повторяет форму скобы с противоположной стороны
        const double inner = side * (radius - 22);	// Находит точку захвата у стенки
        const double outer = side * (radius + 72);	// Задает внешний край скобы
        const double lip = side * (radius + 37);	// Задает внутренний уступ скобы
        drawing.shape({{inner, height - 80}, {outer, height - 80}, {outer, height + 80},	// Задает внешний контур скобы
                       {inner, height + 80}, {inner, height + 39}, {lip, height + 39},	// Формирует нижний захват скобы
                       {lip, height - 39}, {inner, height - 39}}, brush, part);	// Формирует верхний захват скобы
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
const wchar_t* partName(MoldPart part) {	// Сопоставляет элементу подпись исходной схемы
    switch (part) {	// Выбирает название видимой области
        case MoldPart::profit: return L"Прибыль (1)";	// Называет металл выше границы верхней шейки
        case MoldPart::riser: return L"Прибыльная надставка";	// Называет верхнюю надставку
        case MoldPart::upperModel: return L"Верхняя шейка (2)";	// Называет модель верхней шейки
        case MoldPart::funnel: return L"Воронка";	// Называет вход литниковой системы
        case MoldPart::pipe: return L"Литниковая труба";	// Называет вертикальную литниковую трубу
        case MoldPart::upperFrame: return L"Верхняя рамка";	// Называет рамку над кокилем
        case MoldPart::rod: return L"«Сигналка»";	// Называет центральный контрольный стержень
        case MoldPart::plug: return L"Верхняя шейка (2)";	// Называет нижнюю зону модели верхней шейки
        case MoldPart::chill: return L"Кокиль";	// Называет обе боковые стенки бочки
        case MoldPart::barrel: return L"Бочка (3)";	// Называет центральную часть валка
        case MoldPart::lowerModelMiddle: return L"Нижняя шейка, верхняя часть (4)";	// Называет верхнюю часть нижней модели
        case MoldPart::middleFrame: return L"Средняя рамка";	// Называет рамку под кокилем
        case MoldPart::lowerModelBottom: return L"Нижняя шейка, нижняя часть (5)";	// Называет нижнюю часть модели
        case MoldPart::flask: return L"Нижняя опока";	// Называет нижнюю часть литейной оснастки
        case MoldPart::plate: return L"Подопочная плита";	// Называет плиту под сборкой
        case MoldPart::gate: return L"Питатель";	// Называет подвод к полости валка
        case MoldPart::elbow: return L"Колено питателя";	// Называет поворот литникового канала
        case MoldPart::none: return L"";	// Оставляет свободную область без подписи
    }	// Завершает текущий блок обработки
    return L"";	// Защищает вывод при неизвестном идентификаторе
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
void drawMoldLabel(HDC device, const RECT& area, MoldPart part, POINT cursor) {	// Показывает название рядом с указателем
    const wchar_t* text = partName(part);	// Получает название выбранного элемента
    const int length = static_cast<int>(std::wcslen(text));	// Находит длину строки для Win32
    if (length == 0) {	// Проверяет наличие выбранного элемента
        return;	// Оставляет рисунок без подписи
    }	// Завершает текущий блок обработки
    SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));	// Выбирает системный шрифт подписи
    SetBkMode(device, TRANSPARENT);	// Оставляет заливку подсказки под текстом
    SetTextColor(device, RGB(27, 45, 57));	// Задает читаемый темный цвет текста
    SIZE size{};	// Принимает размеры текста в пикселях
    GetTextExtentPoint32W(device, text, length, &size);	// Измеряет подпись выбранного элемента
    const LONG x = std::clamp(cursor.x + 14, area.left + 3, std::max(area.left + 3, area.right - size.cx - 19));	// Вмещает подпись по горизонтали
    const LONG y = std::clamp(cursor.y + 17, area.top + 3, std::max(area.top + 3, area.bottom - size.cy - 15));	// Вмещает подпись по вертикали
    const RECT label{x, y, x + size.cx + 15, y + size.cy + 10};	// Задает поля вокруг подписи
    FillRect(device, &label, GetSysColorBrush(COLOR_INFOBK));	// Закрашивает фон подписи
    FrameRect(device, &label, GetSysColorBrush(COLOR_3DSHADOW));	// Обводит подсказку тонкой рамкой
    TextOutW(device, x + 7, y + 5, text, length);	// Выводит имя возле выбранного элемента
}	// Завершает текущий блок обработки
void drawMold(HDC dc, const RECT& area, const Project& p, MoldHitMap& hits, MoldPart hovered, POINT cursor, const MoldFrame& state, bool rebuild) {	// Рисует пять отделений и литниковый путь
    if (rebuild) { hits.clear(); }	// Обновляет области только при изменении геометрии
    const double frame = outerDiameter(p) / 2;	// Определяет максимальный поперечный габарит оснастки
    const double floor = totalLength(p);	// Получает общую осевую длину валка
    const double pipe = frame + 320;	// Отделяет литниковую трубу от корпуса формы
    const double pipeRadius = p.pipeDiameter / 2;	// Использует заданный внутренний диаметр DS
    const double funnelTop = -450;	// Размещает воронку выше прибыльной надставки
    const double width = area.right - area.left - 44;	// Оставляет горизонтальные поля вокруг чертежа
    const double height = area.bottom - area.top - 36;	// Оставляет вертикальные поля вокруг чертежа
    if (width <= 0 || height <= 0) { return; }	// Пропускает недоступную область рисования
    const double sceneWidth = 2 * frame + 850;	// Учитывает трубу воронку и подопочную плиту
    const double sceneHeight = floor + 650;	// Учитывает верхнюю воронку и нижнюю плиту
    const double scale = std::min(width / sceneWidth, height / sceneHeight);	// Сохраняет одинаковый масштаб по обеим осям
    const int saved = SaveDC(dc);	// Сохраняет графическое состояние родительского окна
    SetBkColor(dc, RGB(218, 227, 233)); SetBkMode(dc, OPAQUE);	// Настраивает фон штриховки оснастки
    const HPEN outline = CreatePen(PS_SOLID, 1, RGB(49, 65, 77));	// Выбирает читаемые контуры деталей
    const HPEN mutedOutline = CreatePen(PS_SOLID, 1, RGB(202, 211, 217));	// Создает приглушенный контур других деталей
    const HBRUSH muted = CreateSolidBrush(RGB(242, 245, 247));	// Осветляет детали вне выделения
    const HBRUSH hatch = CreateHatchBrush(HS_BDIAGONAL, RGB(114, 134, 148));	// Обозначает металлические стенки формы
    const HBRUSH dots = createStipple(dc);	// Обозначает песчаные слои формы
    const HBRUSH paint = CreateSolidBrush(RGB(184, 190, 160));	// Обозначает слой противопригарной краски
    const HBRUSH fastener = CreateSolidBrush(RGB(166, 184, 195));	// Обозначает рамки крепежи и плиту
    const auto empty = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));	// Сохраняет пустую полость до заливки
    const Drawing drawing{dc, scale, area.left + 22 + (width - sceneWidth * scale) / 2 + (frame + 120) * scale, area.top + 18 + (height - sceneHeight * scale) / 2 - funnelTop * scale, area, hits, hovered, outline, mutedOutline, muted, rebuild};	// Задает единое преобразование геометрии в экран
    std::array<double, sectionCount> top{}, bottom{};	// Хранит точные границы пяти отделений
    const MoldPart metalParts[]{MoldPart::profit, MoldPart::upperModel, MoldPart::barrel, MoldPart::lowerModelMiddle, MoldPart::lowerModelBottom};	// Связывает номера с металлическими полостями
    const MoldPart casingParts[]{MoldPart::riser, MoldPart::upperFrame, MoldPart::chill, MoldPart::middleFrame, MoldPart::flask};	// Связывает участки с окружающей оснасткой
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Размещает номера внутри соответствующих полостей
        top[i] = i == 0 ? 0 : bottom[i - 1];	// Продолжает ось без осевого зазора
        bottom[i] = top[i] + p.dimensions[i].height;	// Использует осевой размер соответствующего отделения
        const double radius = p.dimensions[i].diameter / 2.0;	// Вычисляет внутренний радиус полости
        const double layer = radius + p.coating;	// Определяет наружную границу краски
        const double form = layer + (i == 2 ? p.chill : p.sand);	// Окружает бочку кокилем остальные участки песком
        const double outer = form + (i == 2 ? p.barrelFlask : p.flask);	// Добавляет действующую металлическую опоку
        drawing.box(-outer, top[i], i == 4 ? pipe + pipeRadius + 135 : outer, bottom[i], hatch, casingParts[i]);	// Охватывает литники стенками нижней опоки
        drawing.box(-form, top[i], i == 4 ? pipe + pipeRadius + 135 - p.flask : form, bottom[i], i == 2 ? hatch : dots, casingParts[i]);	// Окружает колено и питатель формовочной смесью
        if (p.coating > 0) { drawing.box(-layer, top[i], layer, bottom[i], paint, casingParts[i]); }	// Показывает противопригарное покрытие
        drawing.box(-radius, top[i], radius, bottom[i], empty, metalParts[i]);	// Сохраняет каждую полость отдельным отделением
    }	// Завершает текущий блок обработки
    drawing.box(-frame - 100, floor + 15, pipe + 270, floor + 100, fastener, MoldPart::plate);	// Рисует подопочную плиту под формой
    const double riserOuter = p.dimensions[0].diameter / 2.0 + p.coating + p.sand + p.flask;	// Определяет действующую наружную стенку прибыльной надставки
    drawing.box(-riserOuter - 30, -30, riserOuter + 30, 0, fastener, MoldPart::riser);	// Рисует верхний край прибыльной надставки
    drawFasteners(drawing, std::max(p.dimensions[0].diameter, p.dimensions[1].diameter) / 2.0 + p.coating + p.sand + p.flask, bottom[0], fastener, MoldPart::upperFrame);	// Показывает стык прибыли и верхней шейки
    drawFasteners(drawing, std::max(p.dimensions[3].diameter, p.dimensions[4].diameter) / 2.0 + p.coating + p.sand + p.flask, bottom[3], fastener, MoldPart::flask);	// Показывает разъем нижней опоки
    const double gateY = floor - p.dimensions[4].height * 0.15;	// Вводит металл в нижнюю часть шейки
    const double turnY = gateY - 100;	// Размещает колено выше наклонного питателя
    const double gateRadius = std::min(35.0, p.pipeDiameter / 3);	// Ограничивает ширину условного питателя
    drawing.box(pipe - pipeRadius - 65, -80, pipe + pipeRadius + 65, turnY, hatch, MoldPart::pipe);	// Рисует кожух литниковой трубы
    drawing.box(pipe - pipeRadius, -80, pipe + pipeRadius, turnY, empty, MoldPart::pipe);	// Использует DS для внутреннего канала
    for (double joint : {-80.0, floor * 0.35, floor * 0.65, turnY}) {	// Расставляет соединительные обоймы трубы
        drawing.box(pipe - pipeRadius - 110, joint - 20, pipe + pipeRadius + 110, joint + 20, hatch, MoldPart::pipe);	// Рисует разъемный фланец трубы
        drawing.box(pipe - pipeRadius, joint - 21, pipe + pipeRadius, joint + 21, empty, MoldPart::pipe);	// Сохраняет непрерывность внутреннего канала
    }	// Завершает текущий блок обработки
    drawing.shape({{pipe - 280, funnelTop}, {pipe + 280, funnelTop}, {pipe + pipeRadius + 50, -80}, {pipe - pipeRadius - 50, -80}}, hatch, MoldPart::funnel);	// Рисует наружный контур воронки
    drawing.shape({{pipe - 235, funnelTop}, {pipe + 235, funnelTop}, {pipe + pipeRadius, -80}, {pipe - pipeRadius, -80}}, empty, MoldPart::funnel);	// Соединяет полость воронки с трубой
    const double entry = p.dimensions[4].diameter / 2.0;	// Находит правую стенку нижнего отделения
    drawing.shape({{pipe - 170, turnY - gateRadius}, {entry, gateY - gateRadius}, {entry, gateY + gateRadius}, {pipe - 150, turnY + gateRadius}}, empty, MoldPart::gate);	// Рисует наклонный питатель до нижней шейки
    const auto elbow = [&]() {	// Задает плавное колено литникового пути
        BeginPath(dc); drawing.moveTo({pipe - pipeRadius, turnY - 70});	// Начинает внутреннюю стенку колена
        drawing.curveTo({pipe - pipeRadius, turnY - 20}, {pipe - 120, turnY - gateRadius}, {pipe - 170, turnY - gateRadius});	// Подводит канал к наклонному питателю
        drawing.lineTo({pipe - 150, turnY + gateRadius});	// Соединяет нижние стенки литниковых каналов
        drawing.curveTo({pipe - 70, turnY + gateRadius}, {pipe + pipeRadius, turnY}, {pipe + pipeRadius, turnY - 70});	// Возвращает наружную стенку к трубе
        CloseFigure(dc); EndPath(dc);	// Замыкает внутреннюю полость колена
    };	// Завершает объявление структуры или набора данных
    drawing.style(MoldPart::elbow, empty); elbow(); StrokeAndFillPath(dc);	// Рисует колено с учетом выделения
    if (rebuild) { elbow(); const auto region = PathToRegion(dc); hits.addFluid(region, false); hits.add(region, MoldPart::elbow); }	// Сохраняет область наведения и канал заполнения
    const HBRUSH colors[]{CreateSolidBrush(RGB(45, 112, 181)), CreateSolidBrush(RGB(242, 177, 46)), CreateSolidBrush(RGB(228, 76, 57)), muted};	// Согласует цвета трех зон с легендой
    const auto box = [&](double left, double y1, double right, double y2, HBRUSH brush) {	// Закрашивает область без новых областей наведения
        const auto a = drawing.pixel({left, y1}); const auto b = drawing.pixel({right, y2});	// Переводит физические координаты в экранные
        const RECT rectangle{a.x, a.y, b.x, b.y}; FillRect(dc, &rectangle, brush);	// Заполняет прямоугольный фрагмент металла
    };	// Завершает объявление структуры или набора данных
    if (hits.runners && state.filling > 0 && state.filling < 1) {	// Описывает параметры текущего действия
        const int clipping = SaveDC(dc); SelectClipRgn(dc, hits.runners);	// Ограничивает металл полостями литниковой системы
        const bool selected = hovered == MoldPart::none || hovered == MoldPart::funnel || hovered == MoldPart::pipe || hovered == MoldPart::gate || hovered == MoldPart::elbow;	// Выбирает контраст литникового пути
        const HBRUSH color = colors[selected ? 2 : 3];	// Приглушает путь при выборе другой детали
        const double front = funnelTop + (turnY + 60 - funnelTop) * std::min(1.0, state.filling / 0.14);	// Ведет металл от воронки вниз
        const double drain = std::clamp((state.filling - 0.88) / 0.12, 0.0, 1.0);	// Постепенно опустошает литники перед окончанием заливки
        const double tail = funnelTop + (turnY + 80 - funnelTop) * drain;	// Опускает заднюю границу поступающего металла
        box(pipe - 320, tail, pipe + 320, front, color);	// Покрывает всю ширину воронки включая левый угол
        if (state.filling > 0.14 && state.filling < 0.99) {	// Описывает параметры текущего действия
            const double x = pipe - 175 - (pipe - 175 - entry) * std::min(1.0, (state.filling - 0.14) / 0.06);	// Передвигает фронт к полости валка
            box(x, turnY - 60, pipe - 140, gateY + gateRadius, color);	// Завершает путь перед подъемом уровня
        }	// Завершает текущий блок обработки
        RestoreDC(dc, clipping);	// Возвращает исходные графические ограничения
    }	// Завершает текущий блок обработки
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Размещает номера внутри соответствующих полостей
        const int clipping = SaveDC(dc); SelectClipRgn(dc, hits.casting);	// Ограничивает окраску внутренней полостью отливки
        const double front = bottom[i] - p.dimensions[i].height * state.filled[i];	// Получает уровень заполнения выбранного цилиндра
        const auto a = drawing.pixel({-frame, front}); const auto b = drawing.pixel({frame, bottom[i]});	// Ограничивает еще не заполненную часть
        IntersectClipRect(dc, a.x, a.y, b.x, b.y);	// Оставляет только находящийся внутри металл
        const bool selected = hovered == MoldPart::none || hovered == metalParts[i];	// Выделяет металл выбранного отделения
        box(-frame, front, frame, bottom[i], colors[selected ? 2 : 3]);	// Начинает заполнение с жидкого состояния
        const auto* sample = state.samples[i];	// Получает рассчитанный профиль выбранного момента
        if (sample && state.filling >= 1) {	// Показывает фазы только после заполнения
            const double radius = p.dimensions[i].diameter / 2.0;	// Вычисляет физический радиус данного сечения
            const auto& profile = sample->profile;	// Использует компактные сохраненные температуры
            const auto r = [&](std::size_t k) { return k == 0 ? 0.0 : k + 1 == profile.size() ? radius : radius * (k - 0.5) / (profile.size() - 2); };	// Восстанавливает ось центры колец и стенку
            if (profile.size() >= 2) {	// Проверяет наличие интервалов для фазовой окраски
                const auto zoneAt = [&](std::size_t k) { const double t = (profile[k - 1] + profile[k]) / 2.0; return t <= p.phase.solidus ? 0 : t >= p.phase.liquidus ? 2 : 1; };	// Выбирает состояние интервала температурного профиля
                std::size_t start = 0; int zone = zoneAt(1);	// Начинает непрерывную цветовую полосу на оси
                for (std::size_t k = 2; k <= profile.size(); ++k) {	// Объединяет соседние интервалы одинакового состояния
                    const int next = k == profile.size() ? -1 : zoneAt(k);	// Замыкает последнюю полосу у стенки
                    if (next != zone) {	// Рисует только границу смены состояния
                        box(r(start), top[i], r(k - 1), bottom[i], colors[selected ? zone : 3]);	// Окрашивает правую непрерывную полосу металла
                        box(-r(k - 1), top[i], -r(start), bottom[i], colors[selected ? zone : 3]);	// Окрашивает симметричную левую полосу металла
                        start = k - 1; zone = next;	// Начинает следующую цветовую полосу профиля
                    }	// Завершает текущий блок обработки
                }	// Завершает текущий блок обработки
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        RestoreDC(dc, clipping);	// Возвращает исходные графические ограничения
    }	// Завершает текущий блок обработки
    drawing.box(-8, 0, 8, bottom[1] + p.dimensions[2].height * 0.2, fastener, MoldPart::rod);	// Показывает условный сигнальный стержень
    SetBkMode(dc, TRANSPARENT); SetTextAlign(dc, TA_CENTER | TA_TOP);	// Подготавливает постоянную нумерацию отделений
    SetTextColor(dc, RGB(20, 35, 45));	// Выбирает читаемый цвет цифровых обозначений
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Размещает номера внутри соответствующих полостей
        const auto position = drawing.pixel({-p.dimensions[i].diameter * 0.14, (top[i] + bottom[i]) / 2});	// Отводит номер от сигнального стержня
        const wchar_t number[]{static_cast<wchar_t>(L'1' + i), 0};	// Формирует обозначение от одного до пяти
        TextOutW(dc, position.x, position.y - 7, number, 1);	// Сохраняет номер видимым без наведения мыши
    }	// Завершает текущий блок обработки
    static_cast<void>(cursor); RestoreDC(dc, saved);	// Возвращает контекст вызывающему окну
    for (const auto brush : {colors[0], colors[1], colors[2], muted, hatch, dots, paint, fastener}) { DeleteObject(brush); }	// Освобождает созданные кисти текущего кадра
    DeleteObject(outline); DeleteObject(mutedOutline);	// Освобождает контурные перья
}	// Завершает текущий блок обработки
