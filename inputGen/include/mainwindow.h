#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainwindow.h"
#include "project.h"
#include <QMainWindow>


class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionLoad_SVG_triggered();
    void on_actionSave_points_triggered();

signals:
    void currentProjectUpdated();

protected:
    virtual void closeEvent(QCloseEvent*);

private:
    void readSettings();
    InputGen::Application::Project* _project;
};

#endif // MAINWINDOW_H