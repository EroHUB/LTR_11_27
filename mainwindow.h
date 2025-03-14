#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"
#include <QMainWindow>
#include <QVector>
#include <QDateTime>
#include <QSharedPointer>
#include <QMutex>

#define ACQ_BLOCK_SIZE (50)
#define GRAPH_SIZE (6000000)
#define NSAMPLES (2*LTR27_MEZZANINE_NUMBER*1024)
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    void setupDataTimer();

private slots:
    void on_pushButton_startLtr11_clicked();
    void on_pushButton_meassure_clicked();
    void on_pushButton_ltr27_clicked();

    void on_termistr_2_clicked();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();
    void stab();

    //void onTemperatureUpdated(double temperature); // Слот для обработки температуры

public:
    Ui::MainWindow *ui;
    QCustomPlot *customplot;    // Объявляем графическое полотно
    QCPGraph *graphic;
    QTimer dataTimer;
        // Объявляем график

private:

   // void stab(double t_min, double t_max, double t_now); // Функция стабилизации

    QVector<double> graph_data;
    QVector<double> graph_data_therm;
    QVector<QDateTime> timeStamps; // Вектор для хранения времени
    QVector<double> timeData; // Вектор для хранения времени в числовом формате
    double startTime; // Время начала измерений
    QMutex dataMutex; // Мьютекс для синхронизации доступа к данным
    QTimer *timer; // Таймер
    double tmin, tmax;
};

class waiter : public QObject{
    Q_OBJECT
public:
    void Waiting();

signals:
    void finished(bool);
};

class Drawing : public QObject{
    Q_OBJECT
public:
    void draw(Ui::MainWindow *ui);
    void draw_ltr27(Ui::MainWindow *ui);    // Добавляем объявление функции draw


signals:
    void finished(bool);

};



class CollectData : public QObject { // класс сбора данных
    Q_OBJECT
public slots:
    void Collect();
signals:
    void finished(bool);
};



class Collector : public QObject { //класс сборщика данных
    Q_OBJECT
    CollectData* col_data;
public slots:
    void collect();

signals:
    void finished(bool);
};

class CollectData_ltr27 : public QObject { // класс сбора данных
    Q_OBJECT
public slots:
    void Collect_from_thermistor();
signals:
    void finished(bool);
    void temperatureUpdated(double temperature); // для стабилизации температуры
};



class Collector_ltr27 : public QObject { //класс сборщика данных
    Q_OBJECT
    CollectData_ltr27* col_data;
public slots:
    void collect();

signals:
    void finished(bool);
};




#endif // MAINWINDOW_H
