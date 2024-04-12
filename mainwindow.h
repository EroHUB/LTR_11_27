#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"

#define ACQ_BLOCK_SIZE (8000)
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



private:
    void on_pushButton_startLtr11_clicked();
    void on_pushButton_meassure_clicked();
public:
    Ui::MainWindow *ui;
    QCustomPlot *customplot;    // Объявляем графическое полотно
    QCPGraph *graphic;
    QTimer dataTimer;
        // Объявляем график
};



class Drawing : public QObject{
    Q_OBJECT
public:
    Drawing(Ui::MainWindow *ui, double *buff) : ui(ui), buff_data(buff) {}
    void draw(); // Добавляем объявление функции draw
    Ui::MainWindow *ui;
    double* buff_data;
private:

};



class CollectData : public QObject { // класс сбора данных
    Q_OBJECT
public slots:
    void Collect();
};



class Collector : public QObject { //класс сборщика данных
    Q_OBJECT
    CollectData* col_data;
public slots:
    void collect();
signals:
    void finished(bool);
};



class Controller : public QObject {
    Q_OBJECT
public:
    void makeThread();
};

#endif // MAINWINDOW_H
