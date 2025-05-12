#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltr11api.h"
#include "ltr27api.h"
#include "ltr42api.h"
#include <QFile>
#include <QTextStream>
#include "qcustomplot.h"
#include <QThread>
#include <QString>
#include <QDateTime>
#include <QSharedPointer>
#include <QStringConverter>

using namespace std;

TLTR11 ltr11;
TLTR27 ltr27;
TLTR27 ltr27_2;
TLTR42 ltr42;

double buff_data[ACQ_BLOCK_SIZE];
double therm_data[ACQ_BLOCK_SIZE];
bool waiting_flag = 1;
bool flag_ltr11;
bool flag_ltr27;
bool RunFlag = 0;
int cycle_timer = 500; // частота опроса датчиков
int pulse_length = 400;


//переменные для пид регулятора
int time_pulse_max = 400;
int T_imp;
bool flag_stab = 0;
double delta_sum; // интеграл
double k_prop = 1000;
double k_int = 1000;    // кэф пропорциональности считал как
double Q_max;
double Q_fact;
double T_fact;
double P_out;
double T_stabilize;

QVector<double> T_PID_stab(0);


QVector<double> xData(0);
QVector<double> yData(0);
QVector<double> xData2(0);
QVector<double> yData2(0);
QVector<double> xData3(0);
QVector<double> yData3(0);

QVector<double> graph_data(0);
QVector<double> data_stab(0);
QVector<double> graph_data_therm(0);
QVector<QDateTime> timeStamps; // Вектор для хранения времени
QVector<double> timeData; // Вектор для хранения времени в числовом формате
double startTime = 0; // Время начала измерений
QMutex dataMutex; // Мьютекс для синхронизации доступа к данным

double k = 0.0039 * 100;

//переключатель DT1-DT8
bool DT1 = false;
bool DT2 = false;
bool DT3 = false;
bool DT4 = false;
bool DT5 = false;
bool DT6 = true;
bool DT7 = true;
bool DT8 = true;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Настройка customplot
    ui->customplot->addGraph();
    ui->customplot->addGraph();
    ui->customplot->xAxis->setRange(0, 1000);
    ui->customplot->yAxis->setRange(0, 50);
    ui->customplot->axisRect()->setupFullAxesBox();
    ui->customplot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag | QCP::iSelectPlottables);

    // Настройка termistr
    ui->termistr->addGraph();
    ui->termistr->addGraph();
    ui->termistr->addGraph();
    ui->termistr->xAxis->setRange(0, 1000);
    ui->termistr->yAxis->setRange(0, 50);
    ui->termistr->axisRect()->setupFullAxesBox();
    ui->termistr->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag | QCP::iSelectPlottables);
    ui->termistr->axisRect()->setRangeDrag(Qt::Horizontal);
    ui->termistr->axisRect()->setRangeZoom(Qt::Horizontal);

    // Настройка оси X для отображения времени
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    ui->termistr->xAxis->setTicker(dateTicker);
    dateTicker->setDateTimeFormat("hh:mm:ss");

    // Отступы сверху и снизу
    ui->termistr->yAxis->setRangeLower(0 - 5); // Отступ снизу
    ui->termistr->yAxis->setRangeUpper(50 + 5); // Отступ сверху

    // Цвета и подписи графиков
    ui->termistr->graph(0)->setPen(QPen(QColor(Qt::red)));
    ui->termistr->graph(1)->setPen(QPen(QColor(Qt::blue)));
    ui->termistr->graph(2)->setPen(QPen(QColor(Qt::green)));
    ui->termistr->graph(0)->setName("ДТ8");
    ui->termistr->graph(1)->setName("ДТ7");
    ui->termistr->graph(2)->setName("ДТ6");
    ui->termistr->legend->setVisible(true);

    graph_data.reserve(GRAPH_SIZE);
}

QString createDailyFolderAndFile(int &fileCounter) {
    QDateTime currentDate = QDateTime::currentDateTime();
    QString folderName = currentDate.toString("yyyy-MM-dd");
    QDir dir("C:/Users/maxim/Documents/LTR_try/" + folderName);

    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Формируем имя файла на основе текущего времени
    QString fileName = dir.filePath(currentDate.toString("yyyyMMdd_hhmmss") + ".csv");

    // Если файл уже существует, добавляем счетчик
    if (QFile::exists(fileName)) {
        fileName = dir.filePath(currentDate.toString("yyyyMMdd_hhmmss") +
                                QString("_%1.csv").arg(fileCounter));
        while (QFile::exists(fileName)) {
            fileCounter++;
            fileName = dir.filePath(currentDate.toString("yyyyMMdd_hhmmss") +
                                    QString("_%1.csv").arg(fileCounter));
        }
    }

    return fileName;
}

static int Startltr42(){ //запуск лтр42
    int res;
    const CHAR crate = 0;
    res = LTR42_Init(&ltr42);
    if (res == 0)
    {
        res = LTR42_Open(&ltr42, (int)0, (WORD)0, &crate, (INT)4);
        if (res == 0)
        {
            ltr42.Marks.StartMark_Mode = 0;
            res = LTR42_Config(&ltr42);
        }

    }

    return res;
    delete[]&res;

}

static void startandwrite(WORD Output)
{
    int res;
    res = LTR42_WritePort(&ltr42, Output);
    res;
}
int wait_c = 0;
void waiter::Waiting(){
    wait_c++;
    if(wait_c == 2){
        emit finished(true);
        wait_c = 0;
    }
}

// Функция для преобразования 8-значного числа в строку формата hh:mm:ss




void CollectData::Collect() {
    dataMutex.lock(); // Блокируем доступ к данным
    static int fileCounter = 1; // Счетчик файлов
    static QString currentFileName = createDailyFolderAndFile(fileCounter); // Создаем папку и файл
    static QDate lastDate = QDate::currentDate();

    QDate currentDate = QDate::currentDate();
    if (currentDate != lastDate) {
        // Если день сменился, создаем новую папку и файл
        fileCounter = 1;
        currentFileName = createDailyFolderAndFile(fileCounter);
        lastDate = currentDate;
    }

    double time_out = 8000;
    INT res;
    DWORD data[ACQ_BLOCK_SIZE];
    res = LTR11_Start(&ltr11);

    QFile file(currentFileName);
    if (!file.open(QIODevice::Append)) {
        qDebug() << "Ошибка открытия файла:" << file.errorString();
        dataMutex.unlock();
        return;
    }

    // Создаем кодировщик для Windows-1251
    QStringEncoder encoder(QStringConverter::Encoding::System); // Используем системную кодировку (обычно Windows-1251 на Windows)

    QTextStream stream(&file);

    // Если файл пустой, добавляем заголовок
    if (file.size() == 0) {
        QString header = "Номер замера;Показание;Время\n";
        file.write(encoder.encode(header)); // Записываем заголовок в кодировке Windows-1251
    }

    res = LTR_Recv(&ltr11.Channel, data, NULL, ACQ_BLOCK_SIZE, time_out + 3000);
    res = LTR11_ProcessData(&ltr11, data, buff_data, &res, TRUE, TRUE);

    if (timeStamps.isEmpty()) {
        startTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0; // Начало измерений
    }

    for (int j = 0; j < ACQ_BLOCK_SIZE; j++) {
        QDateTime currentTime = QDateTime::currentDateTime();
        QString str = QString("%1;%2;%3\n") // Используем точку с запятой как разделитель
                          .arg(j + 1) // Номер замера
                          .arg(buff_data[j]) // Показание
                          .arg(currentTime.toString("hh:mm:ss.zzz")); // Время

        if (graph_data.size() >= GRAPH_SIZE) {
            graph_data.pop_back();
            timeStamps.pop_back();
            timeData.pop_back(); // Удаляем старые данные
        }
        graph_data.push_front(buff_data[j]); // Добавляем новые данные
        timeStamps.push_front(currentTime); // Добавляем текущее время
        timeData.push_front((timeStamps.first().toMSecsSinceEpoch() / 1000.0) - startTime); // Преобразуем время в секунды

        // Записываем строку в кодировке Windows-1251
        file.write(encoder.encode(str));
    }

    file.close();
    dataMutex.unlock(); // Разблокируем доступ к данным
}

void CollectData_ltr27::Collect_from_thermistor() {
    dataMutex.lock(); // Блокируем доступ к данным
    static int fileCounter = 1; // Счетчик файлов
    static QString currentFileName = createDailyFolderAndFile(fileCounter); // Создаем папку и файл
    static QDate lastDate = QDate::currentDate();

    QDate currentDate = QDate::currentDate();
    if (currentDate != lastDate) {
        // Если день сменился, создаем новую папку и файл
        fileCounter = 1;
        currentFileName = createDailyFolderAndFile(fileCounter);
        lastDate = currentDate;
    }

    int res;
    DWORD size;
    flag_ltr27 = 1;
    DWORD buf[NSAMPLES];
    size = LTR27_Recv(&ltr27, buf, NULL, 80, 1000);
    double data[size];
    res = LTR27_ProcessData(&ltr27, buf, data, &size, 1, 1);

    QFile file(currentFileName);
    QTextStream stream(&file);

    // Устанавливаем кодировку Windows-1251 для кириллицы
    //stream.setCodec("Windows-1251");

    if (!file.open(QIODevice::Append)) {
        qDebug() << "Ошибка открытия файла:" << file.errorString();
        dataMutex.unlock();
        return;
    }

    // Если файл пустой, добавляем заголовок
    if (file.size() == 0) {
        stream << "Number;DT1;DT2;DT3;DT4;DT5;DT6;DT7;DT8;P_now;T_imp;K_prop;K_int;T_stab;Time\n";
    }

    QDateTime currentTime = QDateTime::currentDateTime();

    // Векторы для хранения сумм и количества измерений для каждого датчика
    std::vector<double> sensorSums(16, 0.0);
    std::vector<int> sensorCounts(16, 0);

    // Собираем данные для каждого датчика
    for (int i = 0; i < size; i++) {
        int sensorIndex = i % 16; // Определяем индекс датчика (0-15)
        data[i] = (data[i] - 100) / k;
        sensorSums[sensorIndex] += data[i];
        sensorCounts[sensorIndex]++;

    }

    // Вектор для хранения средних значений для каждого датчика
    std::vector<double> sensorAverages(16, 0.0);

    // Вычисляем средние значения для каждого датчика
    for (int i = 0; i < 16; i++) {
        if (sensorCounts[i] > 0) {
            sensorAverages[i] = sensorSums[i] / sensorCounts[i];
        }
    }

    // Добавляем средние значения в graph_data_therm
    for (int i = 0; i < 8; i++) {
        graph_data_therm.push_back(sensorAverages[i*2]);
        if (i == 0){
            data_stab.push_back(sensorAverages[i*2]);
            T_PID_stab.push_back(sensorAverages[i*2]);
        }
    }

    // Формируем строку для записи в файл
    QString str = QString("%1").arg(fileCounter); // Номер замера
    for (int i = 0; i < 16; i+=2) {
        str += QString(";%1").arg(sensorAverages[i]); // Средние значения для каждого датчика
    }
    str += QString(";%1").arg(P_out);
    if(T_imp){ // если в переменной есть значение то печатает его, если нет то выводит None
        str += QString(";%1").arg(T_imp);
    } else {
        str += QString(";%1").arg("None");
    }
    str += QString(";%1").arg(k_prop);
    str += QString(";%1").arg(k_int);
    if(T_stabilize){ // если в переменной есть значение то печатает его, если нет то выводит None
        str += QString(";%1").arg(T_stabilize);
    } else {
        str += QString(";%1").arg("None");
    }

    str += QString(";%1\n").arg(currentTime.toString("yyyy-MM-dd hh:mm:ss.zzz")); // Время

    timeStamps.push_back(currentTime); // Добавляем текущее время
    timeData.push_back((currentTime.toMSecsSinceEpoch() / 1000.0) - startTime);

    // Записываем строку в файл
    stream << str;

    file.close();
    fileCounter++; // Увеличиваем счетчик файлов
    dataMutex.unlock(); // Разблокируем доступ к данным
}

void Drawing::draw(Ui::MainWindow *ui) {
    dataMutex.lock(); // Блокируем доступ к данным
    size_t size = graph_data.size();
    QVector<double> xData(size);
    QVector<double> yData(size);

    QVector<double> xData2(size);
    QVector<double> yData2(size);

    // Заполняем данные для графиков
    for (int i = 0; i < size; i++) {
        if (i % 2 == 0) {
            xData[i] = timeData[i]; // Используем время для оси OX
            yData[i] = graph_data[i];
        } else {
            xData2[i] = timeData[i]; // Используем время для оси OX
            yData2[i] = graph_data[i];
        }
    }

    // Устанавливаем данные на график
    ui->customplot->graph(0)->setData(xData, yData);
    ui->customplot->graph(1)->setData(xData2, yData2);

    // Находим минимальное и максимальное значение данных
    double minY = *std::min_element(yData.begin(), yData.end());
    double maxY = *std::max_element(yData.begin(), yData.end());

    // Устанавливаем диапазон оси OY с запасом в 3 единицы
    if (minY == maxY) {
        // Если данные не изменяются, устанавливаем диапазон вручную
        ui->customplot->yAxis->setRange(minY - 3, minY + 3);
    } else {
        ui->customplot->yAxis->setRange(minY - 3, maxY + 3);
    }

    // Настройка оси OX для отображения времени
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    ui->customplot->xAxis->setTicker(dateTicker);

    // Форматируем подписи оси OX как время
    dateTicker->setDateTimeFormat("hh:mm:ss");

    // Устанавливаем диапазон оси OX
    ui->customplot->xAxis->setRange(timeData.first(), timeData.last());

    // Перерисовываем график
    ui->customplot->replot();
    dataMutex.unlock(); // Разблокируем доступ к данным

    emit finished(true);
}

void Drawing::draw_ltr27(Ui::MainWindow *ui) {
    dataMutex.lock();

    // Сначала вычисляем min_count и max_count
    int size = graph_data_therm.size();
    double min_count = 1000;
    double max_count = -1000;

    // Находим минимальное и максимальное значения
    if (DT8 && size >= 8) {
        min_count = qMin(min_count, graph_data_therm[size-8]);
        max_count = qMax(max_count, graph_data_therm[size-8]);
    }
    if (DT7 && size >= 7) {
        min_count = qMin(min_count, graph_data_therm[size-7]);
        max_count = qMax(max_count, graph_data_therm[size-7]);
    }
    if (DT6 && size >= 6) {
        min_count = qMin(min_count, graph_data_therm[size-6]);
        max_count = qMax(max_count, graph_data_therm[size-6]);
    }

    // Проверяем, был ли график приближен
    bool wasZoomed = false;
    if (ui->termistr->axisRect()->rangeZoom() != Qt::Orientations()) {
        QCPRange xRange = ui->termistr->xAxis->range();
        QCPRange yRange = ui->termistr->yAxis->range();
        wasZoomed = (xRange.size() < (timeData.last() - timeData.first())) ||
                    (yRange.lower > min_count || yRange.upper < max_count);
    }

    QVector<double> Empty(0);

    // Обновляем данные графиков
    if (DT8 && size >= 8) {
        yData.insert(0, graph_data_therm[size - 8]);
        xData.insert(0, timeData.last());
        ui->label_3->setText(QString::number(yData.first()));
    }
    if (DT7 && size >= 7) {
        yData2.insert(0, graph_data_therm[size - 7]);
        xData2.insert(0, timeData.last());
        ui->label_4->setText(QString::number(yData2.first()));
    }
    if (DT6 && size >= 6) {
        yData3.insert(0, graph_data_therm[size - 6]);
        xData3.insert(0, timeData.last());
        ui->label_5->setText(QString::number(yData3.first()));
    }

    // Устанавливаем данные для графиков
    ui->termistr->graph(0)->setData(DT8 ? xData : Empty, DT8 ? yData : Empty);
    ui->termistr->graph(1)->setData(DT7 ? xData2 : Empty, DT7 ? yData2 : Empty);
    ui->termistr->graph(2)->setData(DT6 ? xData3 : Empty, DT6 ? yData3 : Empty);

    // Настройка диапазонов осей
    MainWindow* mainWindow = qobject_cast<MainWindow*>(ui->termistr->parent());
    if (!mainWindow || !mainWindow->allowZoom || !wasZoomed) {
        if (!timeData.isEmpty()) {
            double xMin = timeData.first();
            double xMax = timeData.last();
            if (xMax - xMin < 10) xMax = xMin + 10;
            ui->termistr->xAxis->setRange(xMin, xMax);
        }

        double yPadding = qMax(1.0, (max_count - min_count) * 0.1);
        ui->termistr->yAxis->setRange(min_count - yPadding, max_count + yPadding);
    }
    else {
        QCPRange xRange = ui->termistr->xAxis->range();
        QCPRange yRange = ui->termistr->yAxis->range();

        double yPadding = qMax(1.0, (max_count - min_count) * 0.1);
        double newYLower = qMax(yRange.lower, min_count - yPadding);
        double newYUpper = qMin(yRange.upper, max_count + yPadding);

        ui->termistr->xAxis->setRange(xRange);
        ui->termistr->yAxis->setRange(newYLower, newYUpper);
    }

    ui->termistr->replot();
    dataMutex.unlock();
    emit finished(true);
}

void Collector::collect() { //функция запуска сборки данных
    // Этот метод будет запущен при старте потока
    // Аллоцируем наш объект. Теперь это происходит в отдельном потоке
    col_data = new CollectData();
    if(col_data == nullptr) {
        // Если произошла ошибка, то сигнализируем что поток завершен с отрицательным результатом и покидаем функцию (а с ней и завершается поток)
        emit finished(false);
        return;
    }
    // Делаем сложную работу
    col_data->Collect();
    // Сигнализируем об успешном выполнении
    emit finished(true);
}
void Collector_ltr27::collect() { //функция запуска сборки данных
    // Этот метод будет запущен при старте потока
    // Аллоцируем наш объект. Теперь это происходит в отдельном потоке
    col_data = new CollectData_ltr27();
    if(col_data == nullptr) {
        // Если произошла ошибка, то сигнализируем что поток завершен с отрицательным результатом и покидаем функцию (а с ней и завершается поток)
        emit finished(false);
        return;
    }

    col_data->Collect_from_thermistor();
    // Сигнализируем об успешном выполнении
    emit finished(true);
}



void MainWindow::on_pushButton_startLtr11_clicked()
{

    int res;
    res = LTR11_Init(&ltr11);
    res = LTR11_Open(&ltr11, SADDR_DEFAULT, SPORT_DEFAULT, "", 3);
    res = LTR11_GetConfig(&ltr11);
    ui->label->setText("LTR11 started");
    ltr11.StartADCMode = LTR11_STARTADCMODE_INT;
    ltr11.InpMode = LTR11_INPMODE_INT;
    ltr11.LChQnt = 2;
    ltr11.LChTbl[0] = (0 << 6) | (0 << 4) | (0 << 0); // б.о.
    ltr11.LChTbl[1] = (0 << 6) | (0 << 4) | (1 << 0);
    ltr11.ADCMode = LTR11_ADCMODE_ACQ;
    ltr11.ADCRate.prescaler = 8;
    ltr11.ADCRate.divider = 9374;

    res = LTR11_SetADC(&ltr11); //б.о.
    double resD;

    resD = ltr11.ChRate;


}
void MainWindow::on_pushButton_ltr27_clicked()
{
    int res;


    res = LTR27_Init(&ltr27);
    res = LTR27_Open(&ltr27, SADDR_DEFAULT, SPORT_DEFAULT, "", CC_MODULE2);
    res = LTR27_GetConfig(&ltr27);

    res = LTR27_GetDescription(&ltr27, LTR27_ALL_DESCRIPTION );
    if(res==0){ui->label_2->setText("LTR27 started");}
    for(int i=0; i< 8; i++)
        for(int j=0; j<4; j++)
            ltr27.Mezzanine[i].CalibrCoeff[j]= ltr27.ModuleInfo.Mezzanine[i].Calibration[j];
    ltr27.FrequencyDivisor = 99;

    res=LTR27_SetConfig(&ltr27);
    res=LTR27_ADCStart(&ltr27);

}


void MainWindow::on_termistr_2_clicked()
{
    Collector_ltr27 *col_thermistor = new Collector_ltr27();
    QThread* thread_thermistor = new QThread();
    Drawing* dr_thermistor = new Drawing();
    col_thermistor->moveToThread(thread_thermistor);
    connect(thread_thermistor, &QThread::started, col_thermistor, &Collector_ltr27::collect);
    connect(col_thermistor, &Collector_ltr27::finished, dr_thermistor, [=](bool result) { dr_thermistor->draw_ltr27(ui); });
    connect(dr_thermistor, &Drawing::finished, col_thermistor, &Collector_ltr27::collect);
    thread_thermistor->start();
}




void MainWindow::on_pushButton_meassure_clicked()
{

    Collector *col = new Collector();
    QThread* thread = new QThread();
    Drawing* dr = new Drawing();
    col->moveToThread(thread);

    connect(thread, &QThread::started, col, &Collector::collect);


    //connect(col_thermistor, &Collector::finished, )

    connect(col, &Collector::finished, dr, [=](bool result) { dr->draw(ui); });
    connect(dr, &Drawing::finished, col, &Collector::collect);
    thread->start();


    // dr->draw(ui);
    //  dr->draw();
}


MainWindow::~MainWindow()
{
    delete ui;
}



void MainWindow::on_pushButton_clicked()
{   waiter *wait = new waiter();
    QThread* waiting_thread = new QThread();
    // connect(waiting_thread, &QThread::started, wait, &waiter::Waiting);

    wait -> moveToThread(waiting_thread);
    waiting_thread->start();
    Collector_ltr27 *col_thermistor = new Collector_ltr27();
    QThread* thread_thermistor = new QThread();
    Drawing* dr_thermistor = new Drawing();
    col_thermistor->moveToThread(thread_thermistor);
    connect(thread_thermistor, &QThread::started, col_thermistor, &Collector_ltr27::collect);
    /*Для того чтобы оба измерительных прибора выполняли свои функции одновременно реализована следующая схема
    запускается col_thermistor -> dr_thermistor -> wait  -> col_thermistor...
                col            -> dr            /        \  col           ...
    то есть функция wait ждет выполнения dr_thermistor и dr и только после это выполняет col и col_thermistor.
    */

    connect(col_thermistor, &Collector_ltr27::finished, dr_thermistor, [=](bool result) { dr_thermistor->draw_ltr27(ui); });
    connect(dr_thermistor, &Drawing::finished, wait, &waiter::Waiting);
    connect(wait, &waiter::finished, col_thermistor, &Collector_ltr27::collect);


    Collector *col = new Collector();
    QThread* thread = new QThread();
    Drawing* dr = new Drawing();
    col->moveToThread(thread); // тут нужно переделать коннекты как я сделал выше

    connect(thread, &QThread::started, col, &Collector::collect);

    connect(col, &Collector::finished, dr, [=](bool result) { dr->draw(ui); });
    connect(dr, &Drawing::finished, wait, &waiter::Waiting);
    connect(wait, &waiter::finished, col, &Collector::collect);
    //connect(col_thermistor, &Collector::finished, )



    thread_thermistor->start();
    thread->start();
}



void MainWindow::on_pushButton_2_clicked()
{  int res;
    res = Startltr42();
    ui->label_9->setText(QString::number(res));

}


void MainWindow::stab() {
    dataMutex.lock(); // Защищаем доступ к data_stab

    if (data_stab.isEmpty()) {
        qDebug() << "No data in data_stab";
        dataMutex.unlock();
        return;
    }

    double t_now = data_stab.last();
    qDebug() << "Current t_now:" << t_now;

    bool ok_min, ok_max;
    tmin = ui->textEdit->toPlainText().toDouble(&ok_min);
    tmax = ui->textEdit_2->toPlainText().toDouble(&ok_max);

    if (!ok_min || !ok_max) {
        qDebug() << "Ошибка преобразования строки в число";
        dataMutex.unlock();
        return;
    }

    if (t_now < tmin) {
        qDebug() << "min" << t_now;
        startandwrite(pulse_length);
    }
    if (t_now > tmax) {
        qDebug() << "max:" << t_now;
        startandwrite(0);
    }

    dataMutex.unlock(); // Разблокируем доступ к data_stab
}

void MainWindow::on_pushButton_3_clicked() {
    bool ok_min;
    bool ok_max;
    QString tmin_text = ui->textEdit->toPlainText();
    QString tmax_text = ui->textEdit_2->toPlainText();
    tmin = tmin_text.toDouble(&ok_min); // Сохраняем tmin в переменную класса
    tmax = tmax_text.toDouble(&ok_max); // Сохраняем tmax в переменную класса

    if (ok_min && ok_max) {
        ui->label_10->setText("Идет стабилизация");

        // Создаем таймер, если он еще не создан
        if (!timer) {
            timer = new QTimer(this);
            connect(timer, &QTimer::timeout, this, &MainWindow::stab);
        }

        // Запускаем таймер с интервалом 5000 мс (5 секунд)
        timer->start(cycle_timer);
    } else {
        ui->label_10->setText("Ошибка преобра   зования строки");
    }
}
void MainWindow::stab_PID(){
    int pid_time;
    double delta_T = 0.0;

    double T_now = T_PID_stab.last();
    double T_stab = T_stabilize;
    if (T_now + 0.1 > T_stab) {
        flag_stab = true;
    }
    if(flag_stab == true){
        delta_sum += delta_T;
        T_fact = T_PID_stab.last();
        delta_T = T_stabilize - T_fact;
        Q_fact = k_prop * delta_T + k_int * delta_sum;
        if (delta_T <=0){
            pid_time = 0;
        } else{
            pid_time = (Q_fact/Q_max)*cycle_timer;
        }
    } else {
        pid_time = time_pulse_max;
    }
    ui->label_16->setText(QString::number(pid_time));
    //Q_fact = k_prop * delta_T;
    T_imp = pid_time;
    startandwrite(pid_time);
}

void MainWindow::on_pushButton_4_clicked() // это запуск термостабилизации ПИД регулятора
{
    // собираем данные
    QString V = ui->lineEdit->text();
    QString I = ui->lineEdit_2->text();
    QString K_prop = ui->lineEdit_3->text();
    QString K_int = ui->lineEdit_5->text();
    QString T_stab_string = ui->lineEdit_4->text();
    double V_d = V.toDouble();
    double I_d = I.toDouble();
    Q_max = I_d * V_d;
    P_out = Q_max;
    k_prop = K_prop.toDouble();
    k_int = K_int.toDouble()*(cycle_timer/1000);
    T_stabilize = T_stab_string.toDouble();
    ui->label_13->setText(QString::number(Q_max));
    // Создаем таймер, если он еще не создан
    //if (!timerPID) {
        timerPID = new QTimer(this);
        connect(timerPID, &QTimer::timeout, this, &MainWindow::stab_PID);
    //}


    timerPID->start(cycle_timer);

}




void MainWindow::on_checkBox_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT1 = true;
    } else {
        DT1 = false;
    }

}


void MainWindow::on_checkBox_2_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT2 = true;
    } else {
        DT2 = false;
    }
}


void MainWindow::on_checkBox_3_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT3 = true;
    } else {
        DT3 = false;
    }
}


void MainWindow::on_checkBox_4_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT4 = true;
    } else {
        DT4 = false;
    }
}


void MainWindow::on_checkBox_5_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT5 = true;
    } else {
        DT5 = false;
    }
}

void MainWindow::on_checkBox_6_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT6 = true;
    } else {
        DT6 = false;
    }
}


void MainWindow::on_checkBox_7_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT7 = true;
    } else {
        DT7 = false;
    }
}


void MainWindow::on_checkBox_8_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        DT8 = true;
    } else {
        DT8 = false;
    }
}


void MainWindow::on_checkBox_9_stateChanged(int arg1)
{
    allowZoom = (arg1 == Qt::Checked);
}

