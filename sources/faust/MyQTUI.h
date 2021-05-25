#pragma once
#include <faust/gui/GUI.h>
#include <QWidget>

GUI *QTUI_create();
void QTUI_delete(GUI *gui);
QWidget *QTUI_widget(GUI *gui);
