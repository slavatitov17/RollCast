#define NOMINMAX	// Отключает конфликтующие макросы Windows
#include <windows.h>	// Предоставляет защищённую замену файла
#include "project_io.h"	// Подключает проект и рассчитанную историю
#include <fstream>	// Читает и записывает текстовые файлы
#include <iomanip>	// Сохраняет точность числовых параметров
#include <locale>	// Фиксирует десятичную точку файлового формата
#include <stdexcept>	// Передаёт ошибки открытия и сохранения
namespace {	// Скрывает внутренний механизм безопасной записи
template<class Writer>	// Принимает функцию заполнения нового документа
void atomicWrite(const std::filesystem::path& path, Writer writer) {	// Сохраняет документ без повреждения прежнего файла
    auto temporary = path;	// Создаёт временный файл рядом с целевым
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L".tmp";	// Различает временные файлы разных сохранений
    try {	// Обеспечивает уборку незавершённого документа
        std::ofstream stream(temporary, std::ios::binary);	// Открывает собственный временный файл
        stream.exceptions(std::ios::badbit | std::ios::failbit);	// Превращает ошибки записи в исключения
        stream.imbue(std::locale::classic());	// Использует переносимую запись чисел
        stream << std::setprecision(17);	// Сохраняет исходные числа без потери точности
        writer(stream); stream.close();	// Записывает и закрывает полный документ
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {	// Заменяет цель только готовым документом
            throw std::runtime_error("Не удалось сохранить файл");	// Сообщает об отказе окончательной записи
        }	// Завершает замену целевого файла
    } catch (...) { DeleteFileW(temporary.c_str()); throw; }	// Удаляет только собственный незавершённый файл
}	// Завершает защищённое сохранение
}	// Завершает внутренние файловые функции
Project loadProject(const std::filesystem::path& path) {	// Открывает проект пяти отделений монолитного валка
    std::ifstream stream(path, std::ios::binary);	// Поддерживает пути с русскими именами
    stream.imbue(std::locale::classic());	// Читает числа независимо от языка Windows
    std::string magic; int version = 0; stream >> magic >> version;	// Определяет принадлежность и версию документа
    if (magic != "RollCast" || version != 2) {	// Исключает смешение четырёх и пяти отделений
        throw std::runtime_error("Нужен файл .roll версии 2 для пяти отделений");	// Объясняет несовместимость файла другой ветки
    }	// Завершает проверку версии проекта
    Project p;	// Подготавливает независимый кандидат проекта
    for (auto& d : p.dimensions) { stream >> d.diameter >> d.height; }	// Читает пары D1 L1 до D5 L5
    stream >> p.chill >> p.coating >> p.barrelFlask >> p.sand >> p.flask >> p.pipeDiameter;	// Восстанавливает размеры стенок и литниковой трубы
    for (auto& m : p.materials) { stream >> m.density >> m.heatCapacity >> m.conductivity; }	// Читает свойства четырёх материалов
    stream >> p.phase.solidus >> p.phase.liquidus >> p.phase.latentHeat;	// Восстанавливает закон кристаллизации металла
    stream >> p.metalTemperature >> p.formTemperature >> p.airTemperature >> p.externalTransfer;	// Читает температуры и наружную теплоотдачу
    stream >> p.hours >> p.step >> p.diameterStep;	// Восстанавливает горизонт и численную сетку
    if (!stream) { throw std::runtime_error("Проект содержит неполные или повреждённые данные"); }	// Отклоняет обрезанные документы
    validateProject(p); return p;	// Возвращает только физически согласованные входы
}	// Завершает чтение проекта
void saveProject(const std::filesystem::path& path, const Project& p, const Run* run) {	// Сохраняет проект и доступную итоговую сводку
    validateProject(p);	// Проверяет данные до создания документа
    atomicWrite(path, [&](std::ostream& stream) {	// Подготавливает независимый новый файл
        stream << "RollCast 2\n";	// Обозначает новую схему пяти отделений
        for (const auto& d : p.dimensions) { stream << d.diameter << ' ' << d.height << '\n'; }	// Сохраняет номера в порядке сверху вниз
        stream << p.chill << ' ' << p.coating << ' ' << p.barrelFlask << ' ' << p.sand << ' ' << p.flask << ' ' << p.pipeDiameter << '\n';	// Записывает толщины и диаметр трубы
        for (const auto& m : p.materials) { stream << m.density << ' ' << m.heatCapacity << ' ' << m.conductivity << '\n'; }	// Записывает плотность теплоёмкость и теплопроводность
        stream << p.phase.solidus << ' ' << p.phase.liquidus << ' ' << p.phase.latentHeat << '\n';	// Сохраняет свойства фазового перехода
        stream << p.metalTemperature << ' ' << p.formTemperature << ' ' << p.airTemperature << ' ' << p.externalTransfer << '\n';	// Сохраняет тепловые начальные и граничные условия
        stream << p.hours << ' ' << p.step << ' ' << p.diameterStep << '\n';	// Сохраняет настройки численной схемы
        if (run && run->project == p) {	// Добавляет сводку только соответствующего результата
            stream << "SUMMARY\n";	// Отмечает необязательную итоговую часть документа
            for (std::size_t i = 0; i < sectionCount; ++i) {	// Обходит все пять рассчитанных отделений
                const auto& h = run->cooling.section(i);	// Получает историю выбранного отделения
                const auto v = phaseVolumes(p.dimensions[i], h.samples.back());	// Вычисляет итоговые объёмы обеих фаз
                stream << i + 1 << ' ' << v.solid << ' ' << v.liquid << ' ' << h.completedAt.value_or(-1) << '\n';	// Сохраняет объёмы и достигнутое время затвердевания
            }	// Завершает итоговую сводку
        }	// Завершает проверку доступности актуального результата
    });	// Завершает атомарную запись проекта
}	// Завершает сохранение документа
void exportHistory(const std::filesystem::path& path, const Run& run) {	// Сохраняет полную историю всех пяти отделений
    atomicWrite(path, [&](std::ostream& stream) {	// Защищает существующий CSV от ошибок записи
        stream << "\xEF\xBB\xBF";	// Обозначает кодировку русских подписей для Excel
        stream << "Отделение;Время с;Ось °C;Стенка °C;Твёрдая зона м³;Двухфазная зона м³;Жидкая зона м³;Твёрдая фаза м³;Жидкая фаза м³\n";	// Указывает смысл и единицы каждого столбца
        constexpr const char* names[]{"Прибыль (1)", "Верхняя шейка (2)", "Бочка (3)", "Нижняя шейка, верх (4)", "Нижняя шейка, низ (5)"};	// Сохраняет соответствие номеров схеме формы
        for (std::size_t i = 0; i < sectionCount; ++i) {	// Выгружает каждое отделение полностью
            const double volume = cylinderVolume(run.project.dimensions[i]);	// Получает постоянную вместимость отделения
            for (const auto& s : run.cooling.section(i).samples) {	// Сохраняет начальный и все последующие кадры
                const auto v = phaseVolumes(run.project.dimensions[i], s);	// Учитывает фазы внутри двухфазной зоны
                stream << names[i] << ';' << s.seconds << ';' << s.axisTemperature << ';' << s.surfaceTemperature;	// Записывает время и основные температуры
                for (const auto& z : s.phase.zones) { stream << ';' << z.volumeShare * volume; }	// Выгружает три непересекающиеся геометрические зоны
                stream << ';' << v.solid << ';' << v.liquid << '\n';	// Выгружает суммарные объёмы двух фаз
            }	// Завершает полную историю отделения
        }	// Завершает все пять историй
    });	// Завершает защищённую запись CSV
}	// Завершает выгрузку временной истории
