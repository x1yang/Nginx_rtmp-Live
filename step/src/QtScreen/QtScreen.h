#pragma once

#include <QtWidgets/QWidget>
#include "ui_QtScreen.h"

class QtScreen : public QWidget
{
	Q_OBJECT

public:
	QtScreen(QWidget *parent = Q_NULLPTR);

private:
	Ui::QtScreenClass ui;
};
