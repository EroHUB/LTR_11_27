#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltr11api.h"
#include "ltrapi.h"
#include <QFile>
#include <QTextStream>
#include "qcustomplot.h"
#include <QThread>


using namespace std;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->customplot->addGraph();
    ui->customplot->xAxis->setRange(0,1000);
    ui->customplot->yAxis->setRange(0,10);
   QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s");

    ui->customplot->axisRect()->setupFullAxesBox();
    ui->customplot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag |  QCP::iSelectPlottables);

    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
   dataTimer.start(5); // Interval 0 means to refresh as fast as possible
}
TLTR11 ltr11;
double buff_data[ACQ_BLOCK_SIZE];
bool RunFlag = FALSE;
bool block_ready;

void CollectData::Collect(){
    double time_out = 8000;
    int mass_size = (ltr11.ChRate * time_out)* ltr11.LChQnt;
    INT res;

    INT numbdnt = ACQ_BLOCK_SIZE;
    DWORD data[ACQ_BLOCK_SIZE]; // буфер из одного значения в массиве
    double buff_data[ACQ_BLOCK_SIZE]; // буфер из одного значения в массиве
    const DWORD acq_time_out = (DWORD)( 1 / (ltr11.ChRate * ltr11.LChQnt) +
                                        2000);
    res = LTR11_Start(&ltr11);

    QString filename = "C:/Users/maxim/Documents/LTR_try/Data.txt";
    QFile file(filename);
    QTextStream stream(&file);
    file.open(QIODevice::Append);
    while (RunFlag == TRUE) {
        res = LTR_Recv(&ltr11.Channel, data, NULL, ACQ_BLOCK_SIZE, time_out + 1000); /*ошибка 1011 может быть из - за не правильного таймаута,
при выборе таймаута "впритык" программа не успевает провести все вычисления, поэтому нужно увеличить таймаут на 1000*/
        res = LTR11_ProcessData(&ltr11, data, buff_data, &numbdnt , TRUE, TRUE);
        block_ready = TRUE;
        for (int j = 0; j < ACQ_BLOCK_SIZE; j++)
        {
            QString str;
            str = QString::number(buff_data[j]);

            stream << str<<"\n";
        }

        file.close();
    }
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



void Drawing::draw(){ // вывод рисунка
    QVector<double> xData(ACQ_BLOCK_SIZE);
    QVector<double> yData(ACQ_BLOCK_SIZE);
    for (int i = 0; i < ACQ_BLOCK_SIZE; i++) {
        xData[i] = i;        // Значения по оси x от 0 до ACQ_BLOCK_SIZE - 1
        yData[i] = buff_data[i];  // Значения по оси y из массива buff
    }

    ui->customplot->graph(0)->setData(xData, yData);
    ui->customplot->replot();  // Перерисовываем график после обновления данных
    }

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_startLtr11_clicked()
{

    int res;
    res = LTR11_Init(&ltr11);
    res = LTR11_Open(&ltr11, SADDR_DEFAULT, SPORT_DEFAULT, "", 3);
    res = LTR11_GetConfig(&ltr11);
    ui->label->setText("LTR started");
    ltr11.StartADCMode = LTR11_STARTADCMODE_EXTRISE;
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


void MainWindow::on_pushButton_meassure_clicked()
{

    Collector *col = new Collector();
    QThread* thread = new QThread();
    Drawing* dr = new Drawing(ui, buff_data);
    col->moveToThread(thread);
    connect(col, &Collector::finished, dr, &Drawing::draw);
    connect(thread, &QThread::started, col, &Collector::collect);
   // connect(col, SIGNAL(Collect()), dr, SLOT(draw()));
    thread->start();
    dr->draw();



}

