#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltr11api.h"
#include "ltr27api.h"
#include <QFile>
#include <QTextStream>
#include "qcustomplot.h"
#include <QThread>


using namespace std;

TLTR11 ltr11;
TLTR27 ltr27;
TLTR27 ltr27_2;

double buff_data[ACQ_BLOCK_SIZE];
double therm_data[ACQ_BLOCK_SIZE];
bool waiting_flag = 1;
bool flag_ltr11;
bool flag_ltr27;
bool RunFlag = 0;

QVector<double> graph_data(0);
QVector<double> graph_data_therm(0);
double k = 0.0039*100;
double k2 = 0.0039*50;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->customplot->addGraph();
    ui->customplot->addGraph();
    ui->termistr->addGraph();
    ui->termistr->addGraph();
    ui->customplot->xAxis->setRange(0,1000);
    ui->customplot->yAxis->setRange(0,10);

    ui->termistr->xAxis->setRange(0,100);
    ui->termistr->yAxis->setRange(0,50);

    ui->customplot->axisRect()->setupFullAxesBox();
    ui->customplot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag |  QCP::iSelectPlottables);

    ui->termistr->axisRect()->setupFullAxesBox();
    ui->termistr->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag |  QCP::iSelectPlottables);

    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
    dataTimer.start(5); // Interval 0 means to refresh as fast as possible

    graph_data.reserve(GRAPH_SIZE);
}
int wait_c = 0;
void waiter::Waiting(){
    wait_c++;
        if(wait_c == 2){
            emit finished(true);
            wait_c = 0;
        }
}
void CollectData::Collect(){
    double time_out = 8000;
   // int mass_size = (ltr11.ChRate * time_out)* ltr11.LChQnt;
    INT res;

        DWORD data[ACQ_BLOCK_SIZE]; // буфер из одного значения в массиве
        //double buff_data[ACQ_BLOCK_SIZE]; // буфер из одного значения в массиве
        // const DWORD acq_time_out = (DWORD)( 1 / (ltr11.ChRate * ltr11.LChQnt) + 2000);
        res = LTR11_Start(&ltr11);

        QString filename = "C:/Users/maxim/Documents/LTR_try/Data.txt";
        QFile file(filename);
        QTextStream stream(&file);
        file.open(QIODevice::Append);


        res = LTR_Recv(&ltr11.Channel, data, NULL, ACQ_BLOCK_SIZE, time_out + 3000); /*ошибка 1011 может быть из - за не правильного таймаута,
при выборе таймаута "впритык" программа не успевает провести все вычисления, поэтому нужно увеличить таймаут на 1000*/
        res = LTR11_ProcessData(&ltr11, data, buff_data, &res , TRUE, TRUE);

        for (int j = 0; j < ACQ_BLOCK_SIZE; j++)
        {
            QString str;
            str = QString::number(buff_data[j]);
            graph_data.push_front(buff_data[j]);
            stream << str<<"\n";
        }

        file.close();
   // INT numbdnt = ACQ_BLOCK_SIZE;

}
void CollectData_ltr27::Collect_from_thermistor(){
    int res;
    DWORD size;
    flag_ltr27 = 1;
    DWORD buf[NSAMPLES];
    size=LTR27_Recv(&ltr27, buf, NULL, 80, 1000);
        double data[size];
        res=LTR27_ProcessData(&ltr27, buf, data, &size, 1, 1);

        for (int i  = 0; i < size;) {
            for (int var = 0; var < 16; var++) {
                if(var==0){
                    data[i] = (data[i] - 100)/(k);
                    graph_data_therm.push_back(data[i]);

                }
                if(var==2){
                    data[i] = (data[i] - 50)/(k2);
                    graph_data_therm.push_back(data[i]);
                }
                i++;
            }
    }

}
void Drawing::draw_ltr27(Ui::MainWindow *ui){ // вывод рисунка
    size_t size = graph_data_therm.size();
    QVector<double> xData(size);
    QVector<double> yData(size);
    QVector<double> xData2(size);
    QVector<double> yData2(size);
    for (int i = 0; i < size; i++) {
        if(i%2==0){
            xData[i] = i;        // Значения по оси x от 0 до ACQ_BLOCK_SIZE - 1
            yData[i] = graph_data_therm[i];
        }else{
            xData2[i] = i;        // Значения по оси x от 0 до ACQ_BLOCK_SIZE - 1
            yData2[i] = graph_data_therm[i];
        }
    }

    ui->termistr->graph(0)->setData(xData, yData);
    ui->termistr->graph(1)->setData(xData2, yData2);
    ui->termistr->replot();  // Перерисовываем график после обновления данных
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


void Drawing::draw(Ui::MainWindow *ui){ // вывод рисунка
    size_t size = graph_data.size();
    QVector<double> xData(size);
    QVector<double> yData(size);

    QVector<double> xData2(size);
    QVector<double> yData2(size);
    for (int i = 0; i < size; i++) {
        if(i%2==0){
            xData[i] = i;        // Значения по оси x от 0 до ACQ_BLOCK_SIZE - 1
            yData[i] = graph_data[i];
        }
        else{
            xData2[i] = i;        // Значения по оси x от 0 до ACQ_BLOCK_SIZE - 1
            yData2[i] = graph_data[i];
        }

    }

    ui->customplot->graph(0)->setData(xData, yData);
    ui->customplot->graph(1)->setData(xData2, yData2);
    ui->customplot->replot();  // Перерисовываем график после обновления данных
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
    res = LTR27_Open(&ltr27, SADDR_DEFAULT, SPORT_DEFAULT, "", 1);
    res = LTR27_GetConfig(&ltr27);

    res = LTR27_GetDescription(&ltr27, LTR27_ALL_DESCRIPTION );
    if(res==0){ui->label_2->setText("LTR27 started");}
    for(int i=0; i< 8; i++)
        for(int j=0; j<4; j++)
            ltr27.Mezzanine[i].CalibrCoeff[j]= ltr27.ModuleInfo.Mezzanine[i].Calibration[j];
    ltr27.FrequencyDivisor = 199;

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

