import bisect	# Находит соседние расчетные кадры для независимой интерполяции
import json	# Читает исходные числа расчетного ядра
import math	# Проверяет округление и конечность экспортных значений
import pathlib	# Работает только с выделенным каталогом проверки
import re	# Разбирает ссылки серий на ячейки Excel
import subprocess	# Запускает проверочный генератор действующего экспортера
import sys	# Получает пути из команды CTest
import zipfile	# Независимо проверяет двоичный контейнер XLSX
import xml.etree.ElementTree as ET	# Разбирает структуру листов стилей и диаграмм
NS = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main', 'c': 'http://schemas.openxmlformats.org/drawingml/2006/chart'}	# Задает пространства имен стандартных элементов Excel
NAMES = ['Верхняя шейка, верхняя часть (1)', 'Верхняя шейка, нижняя часть (2)', 'Бочка (3)', 'Нижняя шейка, верх (4)', 'Нижняя шейка, низ (5)']	# Сопоставляет актуальные подписи пяти участков
exe, folder = sys.argv[1:]	# Получает исполняемый файл и отдельный каталог проверки
folder = pathlib.Path(folder)	# Ограничивает создаваемые данные каталогом сборки
subprocess.run([exe, str(folder)], check=True)	# Создает книги через настоящий интерфейс C++
def rounded(value):	# Повторяет только правило представления чисел пользователя
    return math.floor(value * 10000 + 0.5) / 10000	# Округляет положительные результаты до четырех знаков
def same(actual, expected):	# Сравнивает число с физическим результатом до округления
    assert isinstance(actual, float) and abs(actual - rounded(expected)) < 1e-8, (actual, expected)	# Исключает текст вместо числа и неверные единицы
def interpolate(samples, time, index):	# Независимо проверяет получасовые значения кривых
    times = [row[0] for row in samples]; right = bisect.bisect_left(times, time)	# Находит точные границы интерполяции
    if right == 0: return samples[0][index]	# Сохраняет начальное состояние без экстраполяции
    if right == len(samples): return samples[-1][index]	# Сохраняет фактический последний кадр
    left, after = samples[right - 1], samples[right]; weight = (time - left[0]) / (after[0] - left[0])	# Вычисляет вес по исходным временным меткам
    return left[index] * (1 - weight) + after[index] * weight	# Не использует функцию интерполяции самого экспортера
for case in ['default', 'irregular', 'partial']:	# Проверяет завершенную неполную и нерегулярную истории
    expected = json.loads((folder / (case + '.json')).read_text())	# Читает независимый снимок результата ядра
    with zipfile.ZipFile(folder / (case + '.xlsx')) as archive:	# Открывает фактически сохраненный файл Excel
        assert archive.testzip() is None	# Исключает ошибки сжатия и контрольных сумм
        xml = lambda path: ET.fromstring(archive.read(path))	# Разбирает только известные элементы формата
        workbook = xml('xl/workbook.xml')	# Получает настоящие имена листов книги
        assert [s.attrib['name'] for s in workbook.findall('s:sheets/s:sheet', NS)] == ['Параметры', 'Объем, м3', 'Толщина, мм']	# Требует ровно три согласованных листа
        strings = [''.join(node.itertext()) for node in xml('xl/sharedStrings.xml').findall('s:si', NS)]	# Восстанавливает русские подписи общей таблицы строк
        styles = xml('xl/styles.xml'); formats = {n.attrib['numFmtId']: n.attrib['formatCode'] for n in styles.findall('s:numFmts/s:numFmt', NS)}	# Находит пользовательский формат четырех знаков
        cell_styles = styles.find('s:cellXfs', NS)	# Связывает числовые ячейки с форматом отображения
        pages = []	# Хранит проверенные значения трех листов
        for page_index in range(1, 4):	# Проверяет каждый лист независимо от отображения
            cells = {}	# Собирает только заполненные ячейки текущего листа
            for cell in xml(f'xl/worksheets/sheet{page_index}.xml').findall('.//s:sheetData/s:row/s:c', NS):	# Читает значения без стороннего Excel парсера
                value = cell.find('s:v', NS)	# Получает сохраненное числовое или строковое значение
                if value is None: continue	# Пропускает пустые объединенные ячейки и границы
                if cell.attrib.get('t') == 's': cells[cell.attrib['r']] = strings[int(value.text)]	# Восстанавливает текст без потери кириллицы
                else:	# Проверяет физические числовые значения отдельно от строк
                    number = float(value.text); assert math.isfinite(number)	# Запрещает недопустимые результаты в готовой книге
                    assert abs(number - rounded(number)) < 1e-8	# Требует реального округления сохраненного числа
                    assert formats[cell_styles[int(cell.attrib['s'])].attrib['numFmtId']] == '0.0000'	# Требует четыре видимых знака в числовых ячейках
                    cells[cell.attrib['r']] = number	# Сохраняет число для дальнейшей физической сверки
            pages.append(cells)	# Фиксирует данные проверенного листа
        same(pages[0]['D11'], expected['volume']); same(pages[0]['E6'], expected['coating'])	# Проверяет общий объем и новое значение SC
        for sheet_index, unit in [(1, 'м3'), (2, 'мм')]:	# Проверяет обе таблицы геометрических слоев
            cells = pages[sheet_index]	# Выбирает таблицу требуемого физического показателя
            assert [cells[f'{c}5'] for c in 'ABCDEF'] == ['Участок', 'Время, с', f'Твердый слой, {unit}', f'Двухфазный слой, {unit}', f'Жидкий слой, {unit}', 'Время затвердевания, с']	# Исключает прежние температурные столбцы и подписи
            row = 6	# Начинает полную историю после заголовков
            for index, section in enumerate(expected['sections']):	# Не ограничивает проверку только первым участком
                for sample in section['samples']:	# Сверяет каждый сохраненный расчетный момент
                    assert cells[f'A{row}'] == NAMES[index]	# Проверяет новое название соответствующего участка
                    same(cells[f'B{row}'], sample[0])	# Сверяет полную временную историю без сокращения
                    for z, letter in enumerate('CDE'): same(cells[f'{letter}{row}'], sample[1 + z + (3 if sheet_index == 2 else 0)])	# Различает объемы и толщины в миллиметрах
                    if section['completed'] is None: assert cells[f'F{row}'] == 'Не достигнуто'	# Не заменяет отсутствие события числовым нулем
                    else: same(cells[f'F{row}'], section['completed'])	# Сверяет отдельное время полного затвердевания
                    row += 1	# Переходит к следующему физическому кадру
            assert f'A{row}' not in cells	# Исключает лишние строки полной истории
        charts = sorted((n for n in archive.namelist() if re.fullmatch(r'xl/charts/chart\d+\.xml', n)), key=lambda n: int(re.search(r'(\d+)\.xml', n).group(1)))	# Получает десять встроенных диаграмм в порядке создания
        assert len(charts) == 10	# Требует пять диаграмм на каждом листе результатов
        for index, path in enumerate(charts):	# Проверяет каждую диаграмму и источник ее чисел
            chart = xml(path); section = expected['sections'][index % 5]; samples = section['samples']	# Находит независимую историю нужного участка
            sheet_index = 1 if index < 5 else 2; cells = pages[sheet_index]	# Выбирает физическую размерность данной диаграммы
            labels = ''.join(chart.itertext())	# Читает заголовки и подписи сохраненных объектов
            assert NAMES[index % 5] in labels and 'Время, с' in labels	# Требует корректное имя участка и ось секунд
            assert ('Объем, м3' if sheet_index == 1 else 'Толщина, мм') in labels	# Проверяет вертикальную единицу измерения
            assert chart.find('.//c:scatterChart', NS) is not None	# Требует настоящую числовую временную ось
            assert any(float(node.attrib['val']) == 1800 for node in chart.findall('.//c:majorUnit', NS))	# Проверяет получасовой шаг делений времени
            duration = samples[-1][0]; target = []	# Формирует независимую сетку графиков
            t = 0	# Начинает график в момент окончания заполнения
            while t < duration: target.append(t); t += 1800	# Добавляет только получасовые моменты
            target.append(duration)	# Сохраняет последний неполный интервал без потери кадра
            series = chart.findall('.//c:scatterChart/c:ser', NS); assert len(series) == 3	# Требует три фазовые кривые каждого участка
            for line, zone in zip(series, [2, 1, 0]):	# Сверяет порядок жидкого двухфазного и твердого слоев
                xref = line.find('c:xVal/c:numRef/c:f', NS).text; yref = line.find('c:yVal/c:numRef/c:f', NS).text	# Получает реальные адреса данных серий
                ranges = []	# Разбирает оба диапазона без догадок о строках
                for ref in [xref, yref]:	# Проверяет адреса времени и соответствующего слоя
                    match = re.fullmatch(r"'([^']+)'!\$([A-Z]+)\$(\d+):\$([A-Z]+)\$(\d+)", ref); assert match, ref	# Требует корректную ссылку на именованный лист
                    assert match[1] == ['Параметры', 'Объем, м3', 'Толщина, мм'][sheet_index] and match[2] == match[4]	# Исключает кривую с другого листа или столбца
                    ranges.append([cells[f'{match[2]}{r}'] for r in range(int(match[3]), int(match[5]) + 1)])	# Читает именно связанные с диаграммой ячейки
                assert len(ranges[0]) == len(target)	# Исключает перенос всех тридцатисекундных кадров на график
                for t, x, y in zip(target, *ranges):	# Сверяет каждый графический момент и физическую величину
                    same(x, t); same(y, interpolate(samples, t, 1 + zone + (3 if sheet_index == 2 else 0)))	# Проверяет последнюю точку и независимую интерполяцию
    print(case + ': структура, все кадры и десять графиков проверены')	# Сообщает короткий результат каждого проверочного сценария
