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
    void shape(const std::vector<Location>& outlinePoints, HBRUSH fill, MoldPart part) const {	// Рисует и регистрирует замкнутый контур
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
        return CreateSolidBrush(RGB(255, 255, 255));	// Возвращает однотонную подложку при ошибке
    }	// Завершает текущий блок обработки
    auto* pixels = static_cast<DWORD*>(data);	// Получает доступ к каждому пикселю
    std::fill_n(pixels, 64, 0xFFFFFF);	// Заполняет образец светлым цветом
    pixels[9] = pixels[45] = 0x202020;	// Расставляет две точки по диагонали
    const HBRUSH brush = CreatePatternBrush(bitmap);	// Создает кисть с повторяемым узором
    DeleteObject(bitmap);	// Освобождает исходный образец после создания кисти
    return brush;	// Передает кисть вызывающей функции
}	// Завершает текущий блок обработки
HBRUSH createBricks(HDC device) {	// Создает черно белый кирпичный узор плиты
    BITMAPINFO texture{};	// Подготавливает заголовок образца кисти
    texture.bmiHeader = {sizeof(BITMAPINFOHEADER), 24, -12, 1, 32, BI_RGB, 0, 0, 0, 0, 0};	// Задает повторяемый рисунок размером двадцать четыре
    void* data = nullptr;	// Принимает память пикселей образца
    const HBITMAP bitmap = CreateDIBSection(device, &texture, DIB_RGB_COLORS, &data, nullptr, 0);	// Создает поверхность кирпичного узора
    if (!bitmap) { return CreateSolidBrush(RGB(255, 255, 255)); }	// Сохраняет белый фон при нехватке ресурсов
    auto* pixels = static_cast<DWORD*>(data);	// Открывает доступ к строкам образца
    for (int y = 0; y < 12; ++y) {	// Обходит два ряда кирпичей
        for (int x = 0; x < 24; ++x) {	// Рисует горизонтальные и смещенные вертикальные швы
            const bool joint = y % 6 == 0 || (x + (y < 6 ? 0 : 6)) % 12 == 0;	// Смещает швы соседних рядов на полкирпича
            pixels[y * 24 + x] = joint ? 0x202020 : 0xFFFFFF;	// Оставляет черные швы на белом фоне
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    const HBRUSH brush = CreatePatternBrush(bitmap);	// Создает повторяемую кисть подопочной плиты
    DeleteObject(bitmap); return brush;	// Освобождает образец и передает готовую кисть
}	// Завершает текущий блок обработки
double sectionRadius(const Project& p, std::size_t i, double t) {	// Определяет контур схемы по текущей геометрии
    const auto radius = [&](std::size_t k) { return p.dimensions[k].diameter / 2.0; };	// Читает номинальные диаметры пяти расчетных цилиндров
    const double local = radius(i);	// Сохраняет номинальный радиус выбранного участка
    if (i <= 1) {	// Соединяет прибыль с верхней шейкой
        const double neck = local * 0.84;	// Сохраняет вертикальные стенки прибыли и верхней шейки
        const double blend = i == 1 ? std::clamp((t - 0.78) / 0.22, 0.0, 1.0) : 0;	// Выделяет скругленный переход к бочке
        return neck + (radius(2) - neck) * blend * blend;	// Раскрывает заделку к верхнему торцу бочки
    }	// Завершает текущий блок обработки
    if (i == 3) {	// Соединяет бочку с нижней шейкой
        const double blend = std::clamp(t / 0.22, 0.0, 1.0);	// Выделяет короткое скругление нижнего перехода
        const double neck = local * 0.9;	// Сохраняет вертикальную стенку нижней шейки
        return neck + (radius(2) - neck) * (1 - blend) * (1 - blend);	// Сохраняет касание бочки и плавный уступ
    }	// Завершает текущий блок обработки
    if (i == 4) { return local * 0.9; }	// Продолжает прямую стенку до нижнего конца
    return local;	// Оставляет рабочую бочку цилиндрической
}	// Завершает текущий блок обработки
std::vector<Location> sectionContour(const Project& p, std::size_t i, double top, double bottom, double inner = -1, double outer = 1) {	// Строит изменяемый контур полости либо полосы
    std::vector<Location> points;	// Собирает обе стороны продольного сечения
    for (int k = 0; k <= 20; ++k) {	// Аппроксимирует правую сторону плавного перехода
        const double t = k / 20.0;	// Задает относительную высоту точки
        points.push_back({outer * sectionRadius(p, i, t), top + (bottom - top) * t});	// Добавляет точку внешней границы полосы
    }	// Завершает текущий блок обработки
    for (int k = 20; k >= 0; --k) {	// Возвращается по противоположной стороне полосы
        const double t = k / 20.0;	// Сохраняет соответствие противоположных точек
        points.push_back({inner * sectionRadius(p, i, t), top + (bottom - top) * t});	// Замыкает внутреннюю границу полосы
    }	// Завершает текущий блок обработки
    return points;	// Передает готовый контур для GDI
}	// Завершает текущий блок обработки
void drawFasteners(const Drawing& drawing, double radius, double height, HBRUSH metal, HBRUSH bracket, MoldPart part, double rightRadius = -1, bool foot = false) {	// Собирает скобу и выступы единого разъема
    for (double side : {-1.0, 1.0}) {	// Зеркально повторяет узел на обеих сторонах
        const double wall = side > 0 && rightRadius >= 0 ? rightRadius : radius;	// Использует действующую наружную стенку нужной стороны
        const auto local = [&](std::initializer_list<Location> contour, HBRUSH fill) {	// Привязывает все детали к одной поверхности
            std::vector<Location> points;	// Подготавливает зеркальный контур крепления
            for (const auto point : contour) { points.push_back({side * (wall + point.x), height + point.y}); }	// Применяет общую опорную точку стенки
            drawing.shape(points, fill, part);	// Регистрирует скобу вместе с соответствующей опокой
        };	// Завершает объявление преобразования координат узла
        if (foot) {	// Устанавливает нижнюю лапу непосредственно на плиту
            local({{-10, -100}, {72, -100}, {82, -90}, {82, 0}, {-10, 0}, {-10, -28}, {48, -28}, {48, -64}, {-10, -64}}, bracket);	// Рисует опорную скобу без зазора снизу
            local({{-55, -74}, {0, -74}, {32, -56}, {48, -48}, {48, -28}, {-55, -28}}, metal);	// Соединяет нижний выступ со скобой
        } else {	// Соединяет две половины разъемной опоки
            local({{-10, -82}, {72, -82}, {82, -72}, {82, 72}, {72, 82}, {-10, 82}, {-10, 48}, {44, 48}, {48, 44}, {48, -44}, {44, -48}, {-10, -48}}, bracket);	// Повторяет скобу со смягченными наружными углами
            local({{-55, -54}, {0, -54}, {32, -26}, {48, -12}, {48, 12}, {32, 26}, {0, 54}, {-55, 54}}, metal);	// Заводит выступы стенки внутрь охватывающей скобы
            drawing.style(part, metal);	// Выбирает общий контур двух половин разъема
            drawing.moveTo({side * (wall - 55), height});	// Начинает разделитель внутри металлической стенки
            drawing.lineTo({side * (wall + 48), height});	// Доводит линию разъема до захвата скобы
        }	// Завершает выбор типа крепежного узла
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
    const double frame = outerDiameter(p) / 2;	// Определяет наибольший радиус основной формы
    const double floor = totalLength(p);	// Суммирует осевые размеры пяти участков
    const double pipeRadius = p.pipeDiameter / 2;	// Сохраняет действующий радиус литниковой трубы
    const double pipeSand = std::max(25.0, p.sand * 0.35);	// Обозначает футеровку внутри металлического стояка
    const double pipeWall = std::max(15.0, p.flask);	// Определяет толщину наружной стенки стояка
    const double pipeOuter = pipeRadius + pipeSand + pipeWall;	// Учитывает канал футеровку и наружную стенку
    const double pipe = frame + pipeOuter + 140;	// Оставляет просвет между трубой и корпусом
    const double funnelTop = -450;	// Размещает воронку выше прибыльной надставки
    const double base = std::max(50.0, p.dimensions[4].height * 0.08);	// Оставляет формовочную смесь под нижней шейкой
    const double minX = -frame - 130;	// Вмещает крепления и левый край плиты
    const double maxX = pipe + std::max(290.0, pipeOuter + 140);	// Вмещает воронку фланцы и правый край
    const double width = area.right - area.left - 32;	// Оставляет небольшие горизонтальные поля схемы
    const double height = area.bottom - area.top - 28;	// Оставляет небольшие вертикальные поля схемы
    if (width <= 0 || height <= 0) { return; }	// Не рисует в недоступной области окна
    const double sceneWidth = maxX - minX;	// Вычисляет полный габарит изменяемой сборки
    const double sceneHeight = floor + base + 70 - funnelTop;	// Учитывает воронку нижнюю форму и плиту
    const double scale = std::min(width / sceneWidth, height / sceneHeight);	// Сохраняет пропорции при вписывании схемы
    const int saved = SaveDC(dc);	// Сохраняет графическое состояние родительского окна
    SetBkColor(dc, RGB(255, 255, 255)); SetBkMode(dc, OPAQUE);	// Оставляет белые промежутки между черными штрихами
    const HPEN outline = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));	// Обводит детали черным контуром
    const HPEN mutedOutline = CreatePen(PS_SOLID, 1, RGB(205, 205, 205));	// Осветляет невыбранные детали при наведении
    const HBRUSH muted = CreateSolidBrush(RGB(246, 246, 246));	// Приглушает фон остальных деталей
    const HBRUSH hatch = CreateHatchBrush(HS_BDIAGONAL, RGB(0, 0, 0));	// Обозначает металлические стенки диагональными штрихами
    const HBRUSH dots = createStipple(dc);	// Обозначает формовочную смесь точечным узором
    const HBRUSH paint = CreateSolidBrush(RGB(40, 40, 40));	// Показывает тонкий противопригарный слой темным
    const HBRUSH fastener = CreateHatchBrush(HS_FDIAGONAL, RGB(0, 0, 0));	// Меняет направление штрихов на креплениях
    const HBRUSH clamp = CreateSolidBrush(RGB(205, 205, 205));	// Отличает литую скобу от штрихованных выступов
    const HBRUSH refractory = CreateSolidBrush(RGB(255, 255, 255));	// Отделяет кладку воронки от полости заливки
    const HBRUSH bricks = createBricks(dc);	// Обозначает плиту кирпичным узором образца
    const auto empty = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));	// Оставляет полости белыми до начала заливки
    const Drawing drawing{dc, scale, area.left + 16 + (width - sceneWidth * scale) / 2 - minX * scale, area.top + 14 + (height - sceneHeight * scale) / 2 - funnelTop * scale, area, hits, hovered, outline, mutedOutline, muted, rebuild};	// Вписывает сборку единым преобразованием координат
    std::array<double, sectionCount> top{}, bottom{};	// Хранит точные осевые границы пяти участков
    const MoldPart metalParts[]{MoldPart::profit, MoldPart::upperModel, MoldPart::barrel, MoldPart::lowerModelMiddle, MoldPart::lowerModelBottom};	// Связывает полости с прежними номерами участков
    const MoldPart casingParts[]{MoldPart::riser, MoldPart::upperFrame, MoldPart::chill, MoldPart::middleFrame, MoldPart::flask};	// Сохраняет названия окружающей оснастки
    const double barrelOuter = p.dimensions[2].diameter / 2.0 + p.coating + p.chill + p.barrelFlask;	// Сохраняет независимую толщину кокиля вокруг бочки
    const double shoulder = p.dimensions[2].diameter / 2.0 + p.coating + p.flask + 20;	// Вмещает переход к бочке внутри стенок
    const double upperOuter = std::max(std::max(shoulder, barrelOuter - 60), std::max(p.dimensions[0].diameter, p.dimensions[1].diameter) / 2.0 + p.coating + p.sand + p.flask);	// Соединяет верхние стенки без случайных уступов
    const double lowerOuter = std::max(std::max(shoulder, barrelOuter - 55), std::max(p.dimensions[3].diameter, p.dimensions[4].diameter) / 2.0 + p.coating + p.sand + p.flask);	// Соединяет нижние стенки общей опоки
    const double bottomRight = pipe + pipeOuter + 55;	// Охватывает колено правой стенкой нижней опоки
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Строит пять отделений по текущим размерам
        top[i] = i == 0 ? 0 : bottom[i - 1];	// Продолжает ось без осевых зазоров
        bottom[i] = top[i] + p.dimensions[i].height;	// Применяет длину собственного участка
        const double outer = i <= 1 ? upperOuter : i == 2 ? barrelOuter : lowerOuter;	// Выбирает наружную стенку соответствующей сборки
        const double wall = i == 2 ? p.barrelFlask : p.flask;	// Учитывает введенную толщину металлической оболочки
        const double right = i == 4 ? bottomRight : outer;	// Расширяет нижнюю опоку вокруг литников
        const double low = bottom[i] + (i == 4 ? base : 0);	// Оставляет песчаное основание под полостью
        drawing.box(-outer, top[i], right, low, hatch, casingParts[i]);	// Рисует штрихованные наружные стенки формы
        drawing.box(-outer + wall, top[i], right - wall, low - (i == 4 ? wall : 0), i == 2 ? hatch : dots, casingParts[i]);	// Заполняет песчаные части точками кокиль штрихами
        auto coating = sectionContour(p, i, top[i], bottom[i]);	// Получает контур модели данного участка
        for (auto& point : coating) { point.x += point.x < 0 ? -p.coating : p.coating; }	// Отодвигает наружную границу покрытия от металла
        if (p.coating > 0) { drawing.shape(coating, paint, casingParts[i]); }	// Показывает покрытие по обеим сторонам модели
        drawing.shape(sectionContour(p, i, top[i], bottom[i]), empty, metalParts[i]);	// Рисует сужения и скругления полости отливки
    }	// Завершает текущий блок обработки
    drawing.box(minX, floor + base, maxX, floor + base + 40, bricks, MoldPart::plate);	// Рисует широкую плиту с кирпичными швами
    drawing.box(-lowerOuter - 82, floor + base - 50, bottomRight + 82, floor + base, hatch, MoldPart::flask);	// Показывает подошву нижней опоки над плитой
    drawing.shape({{-upperOuter - 165, 0}, {-upperOuter - 165, -85}, {-sectionRadius(p, 0, 0), -35}, {-sectionRadius(p, 0, 0), 0}}, refractory, MoldPart::riser);	// Оставляет выступающую левую планку белой как образец
    drawing.box(upperOuter - std::max(45.0, p.flask), -65, upperOuter + 25, 0, hatch, MoldPart::riser);	// Соединяет правый верхний фланец со стенкой
    drawFasteners(drawing, upperOuter, bottom[0], hatch, clamp, MoldPart::upperFrame);	// Размещает верхний разъем и парные скобы
    drawFasteners(drawing, lowerOuter, bottom[3], hatch, clamp, MoldPart::flask, bottomRight);	// Размещает разъем нижней опоки
    drawing.box(-barrelOuter - 25, top[2] - 30, -upperOuter + 25, top[2] + 30, hatch, MoldPart::chill);	// Показывает верхний выступ левой половины кокиля
    drawing.box(upperOuter - 25, top[2] - 30, barrelOuter + 25, top[2] + 30, hatch, MoldPart::chill);	// Повторяет выступ на правой половине кокиля
    drawFasteners(drawing, lowerOuter, floor + base, hatch, clamp, MoldPart::flask, bottomRight, true);	// Показывает крепление нижней опоки у плиты
    const double gateY = floor - p.dimensions[4].height * 0.13;	// Подводит питатель к нижнему концу шейки
    const double turnY = gateY - std::min(130.0, p.dimensions[4].height * 0.3);	// Сохраняет колено внутри нижней опоки
    const double gateRadius = std::min(30.0, p.pipeDiameter / 3);	// Ограничивает ширину питателя размерами стояка
    drawing.box(pipe - pipeOuter, -80, pipe + pipeOuter, turnY, hatch, MoldPart::pipe);	// Рисует наружную металлическую стенку литниковой трубы
    drawing.box(pipe - pipeRadius - pipeSand, -80, pipe + pipeRadius + pipeSand, turnY, dots, MoldPart::pipe);	// Показывает точечную футеровку литниковой трубы
    drawing.box(pipe - pipeRadius, -80, pipe + pipeRadius, turnY, empty, MoldPart::pipe);	// Оставляет канал стояка открытым и непрерывным
    for (double joint : {-80.0, floor * 0.28, floor * 0.58}) {	// Повторяет три фланца на высоте стояка
        drawing.box(pipe - pipeOuter - 65, joint - 20, pipe + pipeOuter + 65, joint + 20, fastener, MoldPart::pipe);	// Показывает выступающие разъемы литниковой трубы
        drawing.box(pipe - pipeRadius - pipeSand, joint - 21, pipe + pipeRadius + pipeSand, joint + 21, dots, MoldPart::pipe);	// Продолжает футеровку через фланец
        drawing.box(pipe - pipeRadius, joint - 21, pipe + pipeRadius, joint + 21, empty, MoldPart::pipe);	// Соединяет белые полости соседних секций
    }	// Завершает текущий блок обработки
    const double funnelHalf = std::max(280.0, pipeOuter + 60);	// Раскрывает воронку над наружной трубой
    drawing.shape({{pipe - funnelHalf, funnelTop}, {pipe + funnelHalf, funnelTop}, {pipe + pipeRadius + 45, -80}, {pipe - pipeRadius - 45, -80}}, refractory, MoldPart::funnel);	// Рисует наклонные штрихованные стенки воронки
    drawing.shape({{pipe - funnelHalf + 55, funnelTop}, {pipe + funnelHalf - 55, funnelTop}, {pipe + pipeRadius, -80}, {pipe - pipeRadius, -80}}, empty, MoldPart::funnel);	// Оставляет внутреннюю поверхность воронки белой
    drawing.style(MoldPart::funnel, refractory);	// Сохраняет яркость кладки при выборе детали
    for (double side : {-1.0, 1.0}) {	// Повторяет кирпичные швы на обеих стенках
        for (int k = 1; k < 12; ++k) {	// Разбивает наклонную кладку на отдельные кирпичи
            const double t = k / 12.0;	// Находит положение шва вдоль стенки
            const double y = funnelTop + (-80 - funnelTop) * t;	// Сохраняет общую высоту концов кирпичного шва
            const double outside = funnelHalf * (1 - t) + (pipeRadius + 45) * t;	// Находит внешнюю грань наклонной стенки
            const double inside = (funnelHalf - 55) * (1 - t) + pipeRadius * t;	// Находит внутреннюю грань наклонной стенки
            drawing.moveTo({pipe + side * inside, y});	// Начинает шов на внутренней стороне кладки
            drawing.lineTo({pipe + side * outside, y});	// Соединяет грани коротким кирпичным швом
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    const double entry = sectionRadius(p, 4, 0.87);	// Соединяет питатель с действующей наклонной стенкой
    drawing.shape({{pipe - 170, turnY - gateRadius}, {entry - 5, gateY - gateRadius}, {entry - 5, gateY + gateRadius}, {pipe - 150, turnY + gateRadius}}, empty, MoldPart::gate);	// Открывает наклонный канал к нижней шейке
    const auto elbow = [&]() {	// Задает плавное колено литникового пути
        BeginPath(dc); drawing.moveTo({pipe - pipeRadius, turnY - 70});	// Начинает внутреннюю стенку колена
        drawing.curveTo({pipe - pipeRadius, turnY - 20}, {pipe - 120, turnY - gateRadius}, {pipe - 170, turnY - gateRadius});	// Поворачивает поток к наклонному питателю
        drawing.lineTo({pipe - 150, turnY + gateRadius});	// Соединяет нижние стенки литниковых каналов
        drawing.curveTo({pipe - 70, turnY + gateRadius}, {pipe + pipeRadius, turnY}, {pipe + pipeRadius, turnY - 70});	// Возвращает наружную стенку к вертикальной трубе
        CloseFigure(dc); EndPath(dc);	// Замыкает внутренний канал колена
    };	// Завершает объявление структуры или набора данных
    drawing.style(MoldPart::elbow, empty); elbow(); StrokeAndFillPath(dc);	// Рисует закругленный поворот внутри нижней опоки
    if (rebuild) { elbow(); const auto region = PathToRegion(dc); hits.addFluid(region, false); hits.add(region, MoldPart::elbow); }	// Регистрирует поворот для подсказки и заливки
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
        box(pipe - funnelHalf, tail, pipe + funnelHalf, front, color);	// Покрывает всю ширину воронки включая левый угол
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
                        for (double side : {-1.0, 1.0}) {	// Повторяет фазовую полосу симметрично относительно оси
                            const auto contour = sectionContour(p, i, top[i], bottom[i], side * r(start) / radius, side * r(k - 1) / radius);	// Согласует границы фаз с наклонным контуром
                            std::vector<POINT> points;	// Подготавливает экранный контур цветной полосы
                            for (const auto point : contour) { points.push_back(drawing.pixel(point)); }	// Преобразует относительный профиль в экранные точки
                            SelectObject(dc, GetStockObject(NULL_PEN)); SelectObject(dc, colors[selected ? zone : 3]);	// Убирает швы между соседними фазовыми полосами
                            Polygon(dc, points.data(), static_cast<int>(points.size()));	// Окрашивает фазу внутри соответствующего сужения
                        }	// Завершает текущий блок обработки
                        start = k - 1; zone = next;	// Начинает следующую цветовую полосу профиля
                    }	// Завершает текущий блок обработки
                }	// Завершает текущий блок обработки
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        RestoreDC(dc, clipping);	// Возвращает исходные графические ограничения
    }	// Завершает текущий блок обработки
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Восстанавливает разделители поверх цветной заливки
        drawing.style(metalParts[i], static_cast<HBRUSH>(GetStockObject(NULL_BRUSH)));	// Сохраняет цвет металла и черный контур
        std::vector<POINT> points;	// Подготавливает контур выбранного участка
        for (const auto point : sectionContour(p, i, top[i], bottom[i])) { points.push_back(drawing.pixel(point)); }	// Повторяет исходную границу без изменения карты
        Polygon(dc, points.data(), static_cast<int>(points.size()));	// Обводит участок после фазовой окраски
    }	// Завершает текущий блок обработки
    SetBkMode(dc, TRANSPARENT); SetTextAlign(dc, TA_CENTER | TA_TOP);	// Подготавливает постоянную нумерацию отделений
    SetTextColor(dc, RGB(0, 0, 0));	// Выбирает читаемый цвет цифровых обозначений
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Размещает номера внутри соответствующих полостей
        const auto position = drawing.pixel({-p.dimensions[i].diameter * 0.14, (top[i] + bottom[i]) / 2});	// Отводит номер от сигнального стержня
        const wchar_t number[]{static_cast<wchar_t>(L'1' + i), 0};	// Формирует обозначение от одного до пяти
        TextOutW(dc, position.x, position.y - 7, number, 1);	// Сохраняет номер видимым без наведения мыши
    }	// Завершает текущий блок обработки
    static_cast<void>(cursor); RestoreDC(dc, saved);	// Возвращает контекст вызывающему окну
    for (const auto brush : {colors[0], colors[1], colors[2], muted, hatch, dots, paint, fastener, bricks, refractory, clamp}) { DeleteObject(brush); }	// Освобождает созданные кисти текущего кадра
    DeleteObject(outline); DeleteObject(mutedOutline);	// Освобождает контурные перья
}	// Завершает текущий блок обработки
