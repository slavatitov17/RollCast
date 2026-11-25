#ifndef NOMINMAX	// Исключает конфликт макросов Windows со стандартными функциями
#define NOMINMAX	// Сохраняет стандартные имена максимума и минимума
#endif	// Завершает настройку заголовков Windows
#include <windows.h>	// Предоставляет защищенную замену готового файла
#include "project_io.h"	// Подключает входные данные и рассчитанные истории
#include "layer_metrics.h"	// Согласует значения Excel с показателями интерфейса
#include <xlsxwriter.h>	// Создает листы форматирование и диаграммы Excel
#include <algorithm>	// Находит соседние моменты истории для интерполяции
#include <array>	// Хранит фиксированные наборы слоев и цветов
#include <cmath>	// Округляет числа до четырех десятичных знаков
#include <cstdlib>	// Освобождает выходной буфер библиотеки Excel
#include <fstream>	// Записывает готовый двоичный контейнер книги
#include <stdexcept>	// Передает ошибки формирования и сохранения документа
#include <string>	// Хранит русские строки в кодировке UTF8
#include <vector>	// Хранит моменты компактной истории графиков
namespace {	// Скрывает вспомогательные средства экспорта книги
void checked(lxw_error error) {	// Обнаруживает отказ записи ячейки или объекта
    if (error) { throw std::runtime_error(std::string("Ошибка Excel: ") + lxw_strerror(error)); }	// Передает понятную причину отказа библиотеки
}	// Завершает текущий блок обработки
std::string utf8(const wchar_t* value) {	// Переводит подписи интерфейса в кодировку Excel
    const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);	// Определяет размер строки вместе с завершающим нулем
    if (!count) { throw std::runtime_error("Не удалось преобразовать подпись Excel"); }	// Исключает поврежденный русский текст
    std::string result(count, 0);	// Выделяет полный буфер преобразования строки
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), count, nullptr, nullptr);	// Сохраняет кириллицу без зависимости от локали
    result.pop_back(); return result;	// Удаляет завершающий ноль из содержимого строки
}	// Завершает текущий блок обработки
struct Book {	// Управляет библиотечными ресурсами и выходным буфером
    const char* bytes = nullptr;	// Принимает готовый контейнер без промежуточного пользовательского файла
    size_t size = 0;	// Хранит длину готового двоичного контейнера
    lxw_workbook* handle = nullptr;	// Владеет открытой книгой до завершения записи
    Book() {	// Настраивает формирование документа в оперативной памяти
        lxw_workbook_options options{}; options.output_buffer = &bytes; options.output_buffer_size = &size;	// Подключает выходной буфер стандартной библиотеки
        handle = workbook_new_opt(nullptr, &options);	// Создает книгу без зависимости от имени файла
        if (!handle) { throw std::runtime_error("Не удалось создать книгу Excel"); }	// Проверяет выделение памяти документа
    }	// Завершает создание книги
    ~Book() {	// Освобождает ресурсы при успехе и исключении
        if (handle) { lxw_workbook_free(handle); }	// Удаляет незавершенную книгу без создания пользовательского файла
        std::free(const_cast<char*>(bytes));	// Освобождает выходной буфер после использования
    }	// Завершает освобождение библиотечных ресурсов
    void finish() {	// Завершает упаковку всех листов и диаграмм
        auto* active = handle; handle = nullptr;	// Передает закрытию единственное владение книгой
        checked(workbook_close(active));	// Получает готовый архив либо причину отказа
    }	// Завершает текущий блок обработки
};	// Завершает объявление структуры или набора данных
struct Styles {	// Объединяет согласованное оформление всех трех листов
    lxw_format* title;	// Выделяет название листа над таблицей
    lxw_format* elapsed;	// Показывает длительность с накопленными часами и секундами
    lxw_format* header;	// Обозначает серые границы и заголовки столбцов
    lxw_format* text;	// Оформляет подписи параметров и участков
    lxw_format* number;	// Показывает четыре знака после десятичного разделителя
    explicit Styles(lxw_workbook* book) {	// Создает повторно используемые форматы ячеек
        title = workbook_add_format(book); elapsed = workbook_add_format(book); header = workbook_add_format(book);	// Выделяет форматы заголовков и длительности
        text = workbook_add_format(book); number = workbook_add_format(book);	// Выделяет форматы текстовых и числовых данных
        for (auto* format : {title, elapsed, header, text, number}) {	// Настраивает единый читаемый шрифт
            if (!format) { throw std::runtime_error("Недостаточно памяти для оформления Excel"); }	// Проверяет каждый выделенный формат
            format_set_font_name(format, "Calibri"); format_set_font_size(format, 11);	// Согласует таблицы и параметры листов
            format_set_align(format, LXW_ALIGN_VERTICAL_CENTER);	// Центрирует содержимое по высоте строки
        }	// Завершает текущий блок обработки
        format_set_bold(title); format_set_font_size(title, 16);	// Выделяет заголовок листа размером и насыщенностью
        format_set_num_format(elapsed, "[hh]:mm:ss");	// Сохраняет полный счет часов без суточного сброса
        format_set_bold(header); format_set_bg_color(header, 0xE8EDF2); format_set_text_wrap(header);	// Отделяет заголовки от числовых данных
        for (auto* format : {header, text, number, elapsed}) { format_set_border(format, LXW_BORDER_THIN); format_set_border_color(format, 0xCCCCCC); }	// Рисует тонкие серые границы заполненных ячеек
        format_set_num_format(number, "0.0000");	// Фиксирует требуемую точность отображения всех чисел
    }	// Завершает создание форматов
};	// Завершает объявление структуры или набора данных
void text(lxw_worksheet* sheet, lxw_row_t row, lxw_col_t col, const std::string& value, lxw_format* format) {	// Записывает строку с проверкой результата
    checked(worksheet_write_string(sheet, row, col, value.c_str(), format));	// Сохраняет текст без неявного преобразования в формулу
}	// Завершает текущий блок обработки
void number(lxw_worksheet* sheet, lxw_row_t row, lxw_col_t col, double value, lxw_format* format) {	// Округляет выгружаемое число без изменения исходного результата
    if (!std::isfinite(value)) { throw std::runtime_error("Результат содержит недопустимое число"); }	// Исключает поврежденные числовые данные
    checked(worksheet_write_number(sheet, row, col, std::round(value * 10000) / 10000, format));	// Сохраняет числовую ячейку с четырьмя знаками
}	// Завершает текущий блок обработки
void completion(lxw_worksheet* page, lxw_row_t row, std::optional<double> seconds, const Styles& style) {	// Размещает длительность события рядом с отдельной таблицей
    text(page, row, 6, "Время затвердевания, с", style.header);	// Оставляет требуемую подпись над значением длительности
    if (seconds) { checked(worksheet_write_number(page, row + 1, 6, std::round(*seconds) / 86400, style.elapsed)); }	// Записывает числовое время Excel с секундным округлением
    else { text(page, row + 1, 6, "Не достигнуто", style.text); }	// Сохраняет отсутствие события вместо нулевого времени
}	// Завершает размещение времени затвердевания
lxw_worksheet* sheet(lxw_workbook* book, const char* name, const Styles& style, bool results) {	// Создает лист без поясняющей строки под заголовком
    auto* result = workbook_add_worksheet(book, name);	// Добавляет лист с требуемым русским названием
    if (!result) { throw std::runtime_error("Не удалось создать лист Excel"); }	// Проверяет допустимость имени и выделение памяти
    worksheet_set_default_row(result, 20, LXW_FALSE); worksheet_set_row(result, 0, 28, nullptr);	// Задает читаемые высоты строк документа
    checked(worksheet_set_column(result, 0, 0, 48, nullptr));	// Вмещает полные названия расчетных участков
    for (lxw_col_t col = 1; col <= 4; ++col) { checked(worksheet_set_column(result, col, col, 18, nullptr)); }	// Сохраняет одинаковую ширину числовых столбцов
    checked(worksheet_merge_range(result, 0, 0, 0, results ? 4 : 3, name, style.title));	// Размещает только название над областью таблиц
    if (results) { worksheet_freeze_panes(result, 5, 0); }	// Закрепляет пять строк без закрепления первого столбца
    worksheet_set_zoom(result, 90); return result;	// Сохраняет исходный масштаб без лишних областей закрепления
}	// Завершает создание базового листа
void parameters(lxw_workbook* book, const Run& run, const Styles& style) {	// Сохраняет действующие входы без служебных резервных полей
    auto* page = sheet(book, "Параметры", style, false);	// Оставляет лист параметров полностью прокручиваемым
    const auto& p = run.project;	// Использует входы именно рассчитанного результата
    const char* labels[]{"Участок", "Диаметр, мм", "Длина, мм", "Объем, м3"};	// Исключает время и покрытие из геометрической сводки
    worksheet_set_row(page, 3, 32, nullptr);	// Вмещает заголовки с единицами измерения
    for (lxw_col_t col = 0; col < 4; ++col) { text(page, 3, col, labels[col], style.header); }	// Подписывает первую таблицу параметров
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Сохраняет участки в порядке схемы
        const auto row = static_cast<lxw_row_t>(4 + i); text(page, row, 0, utf8(sectionNames[i]), style.text);	// Связывает номер с актуальным полным названием
        number(page, row, 1, p.dimensions[i].diameter, style.number); number(page, row, 2, p.dimensions[i].height, style.number);	// Сохраняет все пять пар диаметра и длины
        number(page, row, 3, cylinderVolume(p.dimensions[i]), style.number);	// Показывает постоянный геометрический объем данного участка
    }	// Завершает геометрическую сводку участков
    text(page, 9, 0, "Общие значения (D0, L0, V0)", style.header);	// Отличает наружный диаметр сборки от диаметра металла
    number(page, 9, 1, outerDiameter(p), style.number); number(page, 9, 2, totalLength(p), style.number);	// Показывает общий диаметр и общую осевую длину
    number(page, 9, 3, totalVolume(p), style.number);	// Суммирует металл пяти участков без литников
    const char* inputs[]{"Группа", "Параметр", "Значение", "Единица измерения"};	// Отделяет перечень входов от геометрической сводки
    worksheet_set_row(page, 12, 32, nullptr);	// Вмещает единицы измерения в заголовке параметров
    for (lxw_col_t col = 0; col < 4; ++col) { text(page, 12, col, inputs[col], style.header); }	// Подписывает полный перечень действующих параметров
    lxw_row_t row = 13;	// Начинает последовательную таблицу входных данных
    const auto input = [&](const char* group, const char* label, double value, const char* unit) {	// Записывает параметр с полным названием и единицей
        text(page, row, 0, group, style.text); text(page, row, 1, label, style.text);	// Сохраняет группу и физический смысл значения
        number(page, row, 2, value, style.number); text(page, row++, 3, unit, style.text);	// Сохраняет округленное число в отдельной числовой ячейке
    };	// Завершает вспомогательную запись входного параметра
    input("Геометрия", "Кокиль (SK)", p.chill, "мм"); input("Геометрия", "Слой краски (SC)", p.coating, "мм");	// Расшифровывает размеры кокиля и покрытия
    input("Геометрия", "Опока (SOP)", p.barrelFlask, "мм"); input("Геометрия", "Стенка формы (SFS)", p.sand, "мм");	// Расшифровывает стенки кокильной и песчаной оснастки
    input("Геометрия", "Стенка опоки (SOS)", p.flask, "мм");	// Исключает нерасчетный диаметр стояка из таблицы входов
    input("Геометрия", "Общий диаметр (D0)", outerDiameter(p), "мм"); input("Геометрия", "Общая длина (L0)", totalLength(p), "мм");	// Расшифровывает производные габариты основной формы
    input("Геометрия", "Общий объем (V0)", totalVolume(p), "м3");	// Расшифровывает объем металла без литников
    const char* materials[]{"Металл", "Краска", "Кокиль и опока", "Форма"};	// Сохраняет свойства всех действующих материалов
    for (std::size_t i = 0; i < p.materials.size(); ++i) {	// Сохраняет все редактируемые свойства четырех материалов
        input(materials[i], "Плотность", p.materials[i].density, "кг/м3");	// Сохраняет массу единицы объема материала
        input(materials[i], "Теплоемкость", p.materials[i].heatCapacity, "Дж/(кг·К)");	// Сохраняет коэффициент накопления чувствительной энергии
        input(materials[i], "Теплопроводность", p.materials[i].conductivity, "Вт/(м·К)");	// Сохраняет коэффициент кондуктивного переноса теплоты
    }	// Завершает текущий блок обработки
    input("Металл", "Теплота кристаллизации", p.phase.latentHeat, "Дж/кг");	// Сохраняет скрытую энергию фазового превращения
    input("Металл", "Т солидус", p.phase.solidus, "град C"); input("Металл", "Т ликвидус", p.phase.liquidus, "град C");	// Сохраняет границы интервала кристаллизации
    input("Металл", "Т заливки", p.metalTemperature, "град C");	// Сохраняет начальную температуру расчетного металла
    input("Краска", "Т начальная", p.paintTemperature, "град C");	// Сохраняет начальную температуру покрытия
    input("Кокиль и опока", "Т начальная", p.ironTemperature, "град C");	// Сохраняет начальное состояние металлической оснастки
    input("Кокиль и опока", "Т солидус", p.ironSolidus, "град C"); input("Кокиль и опока", "Т ликвидус", p.ironLiquidus, "град C");	// Сохраняет контрольные пороги нагрева оснастки
    input("Форма", "Т начальная", p.sandTemperature, "град C");	// Сохраняет отдельную начальную температуру песчаной формы
    input("Условия", "Т воздуха", p.airTemperature, "град C"); input("Условия", "Наружная теплоотдача", p.externalTransfer, "Вт/(м2·К)");	// Сохраняет внешнее граничное условие теплообмена
    text(page, row, 0, "Условия", style.text); text(page, row, 1, "Общее время", style.text);	// Отличает автоматическое завершение от числового горизонта
    if (p.autoTime) { text(page, row, 2, "Авто", style.text); } else { number(page, row, 2, p.hours * 60, style.number); }	// Сохраняет выбранный пользователем режим времени
    text(page, row++, 3, "мин", style.text);	// Подписывает единицу заданного общего времени
    input("Условия", "Шаг по времени", p.step, "с"); input("Условия", "Шаг по диаметру", p.diameterStep, "мм");	// Сохраняет параметры пространственной и временной сетки
    checked(worksheet_set_column(page, 1, 1, 33, nullptr)); checked(worksheet_set_column(page, 3, 3, 24, nullptr));	// Вмещает полные названия параметров и единицы измерения
}	// Завершает заполнение листа параметров
std::vector<double> graphTimes(double duration) {	// Выбирает получасовые моменты и последний кадр
    std::vector<double> times;	// Хранит компактную временную сетку графиков
    for (double t = 0; t < duration; t += 1800) { times.push_back(t); }	// Не переносит каждый тридцатисекундный кадр на график
    times.push_back(duration); return times;	// Всегда включает фактический конец расчетного горизонта
}	// Завершает текущий блок обработки
double graphValue(const CoolingResult& history, const SectionDimensions& dimensions, double seconds, unsigned zone, LayerMetric metric) {	// Интерполирует показатель без повторного решения тепловой задачи
    const auto next = std::lower_bound(history.samples.begin(), history.samples.end(), seconds, [](const auto& sample, double time) { return sample.seconds < time; });	// Находит правый соседний кадр расчетной истории
    if (next == history.samples.begin()) { return layerValue(dimensions, *next, zone, metric); }	// Сохраняет начальный момент без экстраполяции
    if (next == history.samples.end()) { return layerValue(dimensions, history.samples.back(), zone, metric); }	// Ограничивает выбор последним сохраненным кадром
    const auto& before = *(next - 1); const double weight = (seconds - before.seconds) / (next->seconds - before.seconds);	// Определяет долю промежутка между двумя кадрами
    return layerValue(dimensions, before, zone, metric) * (1 - weight) + layerValue(dimensions, *next, zone, metric) * weight;	// Согласует произвольный временной шаг с получасовой сеткой
}	// Завершает текущий блок обработки
void history(lxw_workbook* book, const Run& run, const Styles& style, LayerMetric metric) {	// Сохраняет пять таблиц с отдельными временами и графиками
    const char* name = metric == LayerMetric::volume ? "Объем, м3" : "Толщина, мм";	// Выбирает требуемое имя листа результатов
    auto* page = sheet(book, name, style, true);	// Закрепляет только первые пять строк листа результатов
    checked(worksheet_set_column(page, 5, 5, 3, nullptr)); checked(worksheet_set_column(page, 6, 6, 27, nullptr));	// Отделяет время затвердевания пустым столбцом от истории
    checked(worksheet_set_column(page, 7, 7, 3, nullptr));	// Оставляет пустой столбец перед графиком
    for (lxw_col_t col = 8; col < 12; ++col) { checked(worksheet_set_column(page, col, col, 18, nullptr)); }	// Вмещает графики и их компактные исходные таблицы
    const char* labels[]{"Участок", "Время, с", "Твердый слой", "Двухфазный слой", "Жидкий слой"};	// Исключает повторяемое время события из столбцов истории
    const char* unit = metric == LayerMetric::volume ? ", м3" : ", мм";	// Согласует заголовки и физическую размерность значений
    lxw_row_t block = 3;	// Начинает первый участок над пятой закрепленной строкой
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Разделяет полные истории пяти участков
        const auto& result = run.cooling.section(i); const auto label = utf8(sectionNames[i]);	// Сохраняет однозначное соответствие таблицы и участка
        if (result.samples.empty()) { throw std::runtime_error("Нет расчетной истории для экспорта"); }	// Не создает правдоподобную пустую книгу результатов
        checked(worksheet_merge_range(page, block, 0, block, 4, label.c_str(), style.header));	// Выделяет название отдельной таблицы участка
        worksheet_set_row(page, block, 32, nullptr); worksheet_set_row(page, block + 1, 32, nullptr);	// Вмещает заголовки участка и трех слоев
        for (lxw_col_t col = 0; col < 5; ++col) { text(page, block + 1, col, std::string(labels[col]) + (col >= 2 ? unit : ""), style.header); }	// Сохраняет единицы полной истории в секундах
        lxw_row_t row = block + 2;	// Начинает полный набор расчетных кадров участка
        for (const auto& sample : result.samples) {	// Не сокращает таблицу до получасовой сетки графика
            text(page, row, 0, label, style.text); number(page, row, 1, sample.seconds, style.number);	// Сохраняет участок и время после заполнения формы
            for (unsigned z = 0; z < 3; ++z) { number(page, row, static_cast<lxw_col_t>(2 + z), layerValue(run.project.dimensions[i], sample, z, metric), style.number); }	// Сохраняет три геометрических слоя выбранного показателя
            ++row;	// Переходит к следующему исходному расчетному кадру
        }	// Завершает полную историю отдельного участка
        completion(page, block, result.completedAt, style);	// Показывает событие один раз справа от таблицы
        const auto times = graphTimes(result.samples.back().seconds);	// Включает получасовую сетку и точный последний момент
        lxw_row_t dataRow = block + 18;	// Размещает компактные данные ниже собственного графика
        checked(worksheet_merge_range(page, dataRow, 8, dataRow, 11, ("Данные графика: " + label).c_str(), style.header)); ++dataRow;	// Оставляет происхождение кривых доступным для проверки
        worksheet_set_row(page, dataRow, 32, nullptr); text(page, dataRow, 8, "Время, мин", style.header);	// Переводит только графическую временную шкалу в минуты
        for (unsigned z = 0; z < 3; ++z) { text(page, dataRow, static_cast<lxw_col_t>(9 + z), std::string(labels[2 + z]) + unit, style.header); }	// Согласует размерность исходных данных графика
        const lxw_row_t first = ++dataRow;	// Запоминает начало диапазона серий диаграммы
        for (const double t : times) {	// Записывает получасовые моменты и точный конец истории
            number(page, dataRow, 8, t / 60, style.number);	// Сохраняет минуты числовыми ячейками вместо секунд
            for (unsigned z = 0; z < 3; ++z) { number(page, dataRow, static_cast<lxw_col_t>(9 + z), graphValue(result, run.project.dimensions[i], t, z, metric), style.number); }	// Сохраняет интерполированные значения трех геометрических слоев
            ++dataRow;	// Переходит к следующему моменту графика
        }	// Завершает компактную историю диаграммы
        auto* chart = workbook_add_chart(book, LXW_CHART_SCATTER_STRAIGHT);	// Использует настоящую числовую ось времени в минутах
        if (!chart) { throw std::runtime_error("Не удалось создать диаграмму Excel"); }	// Проверяет выделение памяти объекта графика
        const char* names[]{"Твердый слой", "Двухфазный слой", "Жидкий слой"};	// Подписывает серии без температурных кривых
        const lxw_color_t colors[]{0x2D70B5, 0xF2B12E, 0xE44C39};	// Повторяет цвета фаз из окна программы
        for (const unsigned z : {2u, 1u, 0u}) {	// Показывает жидкий двухфазный затем твердый слой
            auto* series = chart_add_series(chart, nullptr, nullptr);	// Создает одну кривую геометрического слоя
            if (!series) { throw std::runtime_error("Не удалось создать серию Excel"); }	// Проверяет выделение памяти кривой
            chart_series_set_name(series, names[z]); chart_series_set_categories(series, name, first, 8, dataRow - 1, 8);	// Задает имя слоя и полный диапазон времени
            chart_series_set_values(series, name, first, static_cast<lxw_col_t>(9 + z), dataRow - 1, static_cast<lxw_col_t>(9 + z));	// Привязывает кривую к компактной таблице своего участка
            lxw_chart_line line{}; line.color = colors[z]; line.width = 1.5; chart_series_set_line(series, &line);	// Сохраняет одинаковую толщину и фазовые цвета линий
        }	// Завершает текущий блок обработки
        chart_title_set_name(chart, label.c_str()); chart_axis_set_name(chart->x_axis, "Время, мин"); chart_axis_set_name(chart->y_axis, name);	// Подписывает участок и обе физические оси
        chart_axis_set_min(chart->x_axis, 0); chart_axis_set_major_unit(chart->x_axis, 30);	// Показывает деления времени через тридцать минут
        chart_axis_set_max(chart->x_axis, std::max(30.0, std::ceil(times.back() / 1800) * 30));	// Вмещает последний момент при неполном получасовом интервале
        chart_axis_set_min(chart->y_axis, 0); chart_legend_set_position(chart, LXW_CHART_LEGEND_BOTTOM);	// Оставляет положительную шкалу и легенду под графиком
        lxw_chart_options options{}; options.x_scale = 1.4; options.y_scale = 1.2;	// Фиксирует размер графика независимо от числа кадров
        checked(worksheet_insert_chart_opt(page, block, 8, chart, &options));	// Ставит график справа от времени данного участка
        block = std::max(row, dataRow) + 3;	// Исключает перекрытие соседних таблиц и графических данных
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
}	// Завершает пространство вспомогательных функций экспорта
void exportWorkbook(const std::filesystem::path& path, const Run& run) {	// Сохраняет готовую книгу без повреждения прежнего файла
    if (run.cooling.sampleCount() == 0) { throw std::runtime_error("Сначала выполните расчет"); }	// Отклоняет выгрузку отсутствующей расчетной истории
    Book book; Styles style(book.handle);	// Создает документ и повторно используемые форматы
    parameters(book.handle, run, style);	// Записывает все входы и итоговую сводку
    history(book.handle, run, style, LayerMetric::volume);	// Сохраняет объемы и пять отдельных диаграмм
    history(book.handle, run, style, LayerMetric::thickness);	// Сохраняет толщины и пять отдельных диаграмм
    book.finish();	// Завершает архив до замены пользовательского документа
    auto temporary = path; temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L".tmp";	// Выбирает собственный временный файл рядом с целью
    try {	// Защищает прежний документ при ошибке диска
        std::ofstream stream(temporary, std::ios::binary); stream.exceptions(std::ios::badbit | std::ios::failbit);	// Превращает ошибки записи в исключения
        stream.write(book.bytes, static_cast<std::streamsize>(book.size)); stream.close();	// Записывает весь сформированный контейнер Excel
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { throw std::runtime_error("Не удалось сохранить книгу Excel"); }	// Заменяет цель только полностью готовым документом
    } catch (...) { DeleteFileW(temporary.c_str()); throw; }	// Удаляет только собственный незавершенный файл
}	// Завершает текущий блок обработки
