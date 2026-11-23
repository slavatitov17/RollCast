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
    lxw_format* note;	// Переносит короткое объяснение структуры выгрузки
    lxw_format* header;	// Обозначает серые границы и заголовки столбцов
    lxw_format* text;	// Оформляет подписи параметров и участков
    lxw_format* number;	// Показывает четыре знака после десятичного разделителя
    explicit Styles(lxw_workbook* book) {	// Создает повторно используемые форматы ячеек
        title = workbook_add_format(book); note = workbook_add_format(book); header = workbook_add_format(book);	// Выделяет форматы заголовков и пояснений
        text = workbook_add_format(book); number = workbook_add_format(book);	// Выделяет форматы текстовых и числовых данных
        for (auto* format : {title, note, header, text, number}) {	// Настраивает единый читаемый шрифт
            if (!format) { throw std::runtime_error("Недостаточно памяти для оформления Excel"); }	// Проверяет каждый выделенный формат
            format_set_font_name(format, "Calibri"); format_set_font_size(format, 11);	// Согласует таблицы и параметры листов
            format_set_align(format, LXW_ALIGN_VERTICAL_CENTER);	// Центрирует содержимое по высоте строки
        }	// Завершает текущий блок обработки
        format_set_bold(title); format_set_font_size(title, 16);	// Выделяет заголовок листа размером и насыщенностью
        format_set_text_wrap(note); format_set_font_color(note, 0x505050);	// Обеспечивает читаемость пояснений без обрезания
        format_set_bold(header); format_set_bg_color(header, 0xE8EDF2); format_set_text_wrap(header);	// Отделяет заголовки от числовых данных
        for (auto* format : {header, text, number}) { format_set_border(format, LXW_BORDER_THIN); format_set_border_color(format, 0xCCCCCC); }	// Рисует тонкие серые границы заполненных ячеек
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
void completion(lxw_worksheet* sheet, lxw_row_t row, lxw_col_t col, std::optional<double> seconds, const Styles& style) {	// Отличает недостигнутое событие от нулевого времени
    if (seconds) { number(sheet, row, col, *seconds, style.number); }	// Сохраняет рассчитанное время затвердевания в секундах
    else { text(sheet, row, col, "Не достигнуто", style.text); }	// Не подменяет отсутствие события числовым нулем
}	// Завершает текущий блок обработки
lxw_worksheet* sheet(lxw_workbook* book, const char* name, const Styles& style, const char* note) {	// Создает лист с заголовком и пояснением
    auto* result = workbook_add_worksheet(book, name);	// Добавляет лист с требуемым русским названием
    if (!result) { throw std::runtime_error("Не удалось создать лист Excel"); }	// Проверяет допустимость имени и выделение памяти
    worksheet_set_default_row(result, 20, LXW_FALSE); worksheet_set_row(result, 0, 28, nullptr);	// Задает читаемые высоты строк документа
    checked(worksheet_set_column(result, 0, 0, 48, nullptr)); for (lxw_col_t col = 1; col <= 4; ++col) { checked(worksheet_set_column(result, col, col, 18, nullptr)); }	// Вмещает длинные названия участков и значения
    checked(worksheet_set_column(result, 5, 5, 25, nullptr));	// Оставляет место правому столбцу времени затвердевания
    checked(worksheet_merge_range(result, 0, 0, 0, 5, name, style.title));	// Выводит название листа над его содержимым
    worksheet_set_row(result, 1, 34, nullptr); checked(worksheet_merge_range(result, 1, 0, 1, 5, note, style.note));	// Объясняет назначение таблицы без дополнительной вкладки
    worksheet_freeze_panes(result, 5, 1); worksheet_repeat_rows(result, 4, 4);	// Закрепляет заголовки и названия участков
    worksheet_set_zoom(result, 90); return result;	// Открывает книгу в удобном исходном масштабе
}	// Завершает текущий блок обработки
void parameters(lxw_workbook* book, const Run& run, const Styles& style) {	// Сохраняет полный снимок входов текущего расчета
    auto* page = sheet(book, "Параметры", style, "Снимок исходных данных завершенного задания. Время затвердевания указано в секундах после заполнения формы.");	// Отделяет результат расчета от редактируемых полей приложения
    const auto& p = run.project;	// Использует входы именно рассчитанного результата
    const char* labels[]{"Участок", "Диаметр, мм", "Длина, мм", "Объем, м3", "Слой краски, мм", "Время затвердевания, с"};	// Задает подписи геометрии и правого итогового столбца
    worksheet_set_row(page, 4, 32, nullptr);	// Вмещает заголовки с единицами измерения
    for (lxw_col_t col = 0; col < 6; ++col) { text(page, 4, col, labels[col], style.header); }	// Подписывает таблицу пяти расчетных участков
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Сохраняет участки в порядке схемы
        const auto row = static_cast<lxw_row_t>(5 + i); text(page, row, 0, utf8(sectionNames[i]), style.text);	// Связывает номер с актуальным полным названием
        number(page, row, 1, p.dimensions[i].diameter, style.number); number(page, row, 2, p.dimensions[i].height, style.number);	// Сохраняет все пять пар D и L
        number(page, row, 3, cylinderVolume(p.dimensions[i]), style.number); number(page, row, 4, p.coating, style.number);	// Показывает объем участка и действующее покрытие
        completion(page, row, 5, run.cooling.section(i).completedAt, style);	// Записывает время события данного участка
    }	// Завершает текущий блок обработки
    text(page, 10, 0, "Всего в форме", style.header); number(page, 10, 2, totalLength(p), style.number);	// Суммирует осевые размеры пяти участков
    number(page, 10, 3, totalVolume(p), style.number); completion(page, 10, 5, run.cooling.completedAt(), style);	// Сохраняет V0 и общее время затвердевания
    const char* inputs[]{"Группа", "Параметр", "Значение", "Единица измерения"};	// Отделяет входные свойства от итоговой сводки
    worksheet_set_row(page, 13, 32, nullptr);	// Вмещает единицы измерения в заголовке параметров
    for (lxw_col_t col = 0; col < 4; ++col) { text(page, 13, col, inputs[col], style.header); }	// Подписывает перечень всех параметров проекта
    lxw_row_t row = 14;	// Начинает последовательную таблицу входных данных
    const auto input = [&](const char* group, const char* label, double value, const char* unit) {	// Записывает один параметр вместе с единицей
        text(page, row, 0, group, style.text); text(page, row, 1, label, style.text);	// Сохраняет группу и физический смысл значения
        number(page, row, 2, value, style.number); text(page, row++, 3, unit, style.text);	// Сохраняет округленное число в отдельной числовой ячейке
    };	// Завершает вспомогательную запись входного параметра
    input("Геометрия", "SK", p.chill, "мм"); input("Геометрия", "SC", p.coating, "мм");	// Сохраняет толщины кокиля и покрытия
    input("Геометрия", "SOP", p.barrelFlask, "мм"); input("Геометрия", "SFS", p.sand, "мм");	// Сохраняет опоку бочки и песчаную стенку
    input("Геометрия", "SOS", p.flask, "мм"); input("Геометрия", "DS", p.pipeDiameter, "мм");	// Сохраняет наружную опоку и диаметр стояка
    input("Геометрия", "D0", outerDiameter(p), "мм"); input("Геометрия", "L0", totalLength(p), "мм");	// Сохраняет производные наружный диаметр и длину
    input("Геометрия", "V0", totalVolume(p), "м3");	// Исключает металл литников из общего объема
    const char* materials[]{"Металл", "Краска", "Кокиль и опока", "Форма"};	// Сохраняет свойства всех действующих материалов
    for (std::size_t i = 0; i < p.materials.size(); ++i) {	// Включает также скрытые постоянные компактного интерфейса
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
    input("Форма", "Т начальная", p.sandTemperature, "град C"); input("Совместимость", "Т формы прежнего формата", p.formTemperature, "град C");	// Сохраняет раздельную и прежнюю начальные температуры
    input("Условия", "Т воздуха", p.airTemperature, "град C"); input("Условия", "Наружная теплоотдача", p.externalTransfer, "Вт/(м2·К)");	// Сохраняет внешнее граничное условие теплообмена
    text(page, row, 0, "Условия", style.text); text(page, row, 1, "Общее время", style.text);	// Отличает автоматическое завершение от числового горизонта
    if (p.autoTime) { text(page, row, 2, "Авто", style.text); } else { number(page, row, 2, p.hours * 60, style.number); }	// Сохраняет выбранный пользователем режим времени
    text(page, row++, 3, "мин", style.text); input("Условия", "Числовой горизонт .roll", p.hours * 60, "мин");	// Сохраняет также резервное значение числового горизонта
    input("Условия", "Шаг по времени", p.step, "с"); input("Условия", "Шаг по диаметру", p.diameterStep, "мм");	// Сохраняет параметры пространственной и временной сетки
    checked(worksheet_set_column(page, 1, 1, 33, nullptr));	// Вмещает полные названия свойств в таблице параметров
}	// Завершает текущий блок обработки
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
void history(lxw_workbook* book, const Run& run, const Styles& style, LayerMetric metric) {	// Записывает полную историю и пять отдельных графиков
    const char* name = metric == LayerMetric::volume ? "Объем, м3" : "Толщина, мм";	// Выбирает требуемое имя листа результатов
    auto* page = sheet(book, name, style, "Таблица содержит все расчетные кадры. Графики используют точки через 1800 с и последний момент; их данные приведены справа под графиками.");	// Объясняет различие полной и компактной историй
    const char* labels[]{"Участок", "Время, с", "Твердый слой", "Двухфазный слой", "Жидкий слой", "Время затвердевания, с"};	// Исключает температуры оси и стенки из выгрузки
    const char* unit = metric == LayerMetric::volume ? ", м3" : ", мм";	// Согласует заголовки и физическую размерность значений
    worksheet_set_row(page, 4, 32, nullptr);	// Вмещает подписи длинных столбцов истории
    for (lxw_col_t col = 0; col < 6; ++col) { text(page, 4, col, std::string(labels[col]) + (col >= 2 && col <= 4 ? unit : ""), style.header); }	// Отделяет название показателя от единицы запятой
    lxw_row_t row = 5;	// Начинает полный набор расчетных кадров
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Сохраняет все пять участков даже при выборе одного
        const auto label = utf8(sectionNames[i]); const auto& result = run.cooling.section(i);	// Использует актуальное имя и соответствующую историю
        if (result.samples.empty()) { throw std::runtime_error("Нет расчетной истории для экспорта"); }	// Не создает правдоподобную пустую книгу результатов
        for (const auto& sample : result.samples) {	// Не сокращает полную таблицу до графической сетки
            text(page, row, 0, label, style.text); number(page, row, 1, sample.seconds, style.number);	// Сохраняет участок и время после заполнения формы
            for (unsigned z = 0; z < 3; ++z) { number(page, row, static_cast<lxw_col_t>(2 + z), layerValue(run.project.dimensions[i], sample, z, metric), style.number); }	// Сохраняет три геометрических слоя выбранного показателя
            completion(page, row++, 5, result.completedAt, style);	// Повторяет итоговое время события в правом столбце
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    checked(worksheet_autofilter(page, 4, 0, row - 1, 5));	// Разрешает фильтрацию полной истории по участкам
    checked(worksheet_set_column(page, 7, 7, 48, nullptr)); checked(worksheet_set_column(page, 8, 11, 18, nullptr));	// Оставляет место графикам и их исходным данным
    lxw_row_t dataRow = 92;	// Размещает компактные таблицы ниже пяти графиков
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Создает отдельную диаграмму каждого расчетного участка
        const auto& result = run.cooling.section(i); const auto label = utf8(sectionNames[i]);	// Сохраняет однозначное соответствие кривых выбранному участку
        const auto times = graphTimes(result.samples.back().seconds);	// Включает получасовую сетку и точный последний момент
        checked(worksheet_merge_range(page, dataRow, 7, dataRow, 11, ("Данные графика: " + label).c_str(), style.header)); ++dataRow;	// Делает происхождение кривых доступным для проверки
        worksheet_set_row(page, dataRow, 32, nullptr);	// Вмещает заголовки компактной таблицы диаграммы
        text(page, dataRow, 8, "Время, с", style.header);	// Подписывает числовую ось компактной таблицы
        for (unsigned z = 0; z < 3; ++z) { text(page, dataRow, static_cast<lxw_col_t>(9 + z), std::string(labels[2 + z]) + unit, style.header); }	// Согласует размерность исходных данных графика
        const lxw_row_t first = ++dataRow;	// Запоминает начало диапазона серий диаграммы
        for (const double t : times) {	// Записывает только разрешенные моменты отображения кривых
            number(page, dataRow, 8, t, style.number);	// Сохраняет время компактной графической сетки
            for (unsigned z = 0; z < 3; ++z) { number(page, dataRow, static_cast<lxw_col_t>(9 + z), graphValue(result, run.project.dimensions[i], t, z, metric), style.number); }	// Сохраняет интерполированные значения трех геометрических слоев
            ++dataRow;	// Переходит к следующему моменту графика
        }	// Завершает текущий блок обработки
        auto* chart = workbook_add_chart(book, LXW_CHART_SCATTER_STRAIGHT);	// Использует настоящую числовую ось времени в секундах
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
        chart_title_set_name(chart, label.c_str()); chart_axis_set_name(chart->x_axis, "Время, с"); chart_axis_set_name(chart->y_axis, name);	// Подписывает участок и обе физические оси
        chart_axis_set_min(chart->x_axis, 0); chart_axis_set_major_unit(chart->x_axis, 1800);	// Показывает деления времени через тридцать минут
        chart_axis_set_max(chart->x_axis, std::max(1800.0, std::ceil(times.back() / 1800) * 1800));	// Вмещает последний момент при неполном получасовом интервале
        chart_axis_set_min(chart->y_axis, 0); chart_legend_set_position(chart, LXW_CHART_LEGEND_BOTTOM);	// Оставляет положительную шкалу и легенду под графиком
        lxw_chart_options options{}; options.x_scale = 1.4; options.y_scale = 1.2;	// Фиксирует размер графика независимо от числа кадров
        checked(worksheet_insert_chart_opt(page, static_cast<lxw_row_t>(4 + i * 16), 7, chart, &options));	// Размещает пять графиков вертикально без перекрытия
        dataRow += 3;	// Разделяет компактные таблицы соседних участков
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
