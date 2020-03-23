#pragma once
#include <QThread>
#include"XData.h"
#include<list>
class XDataThread:public QThread
{
public:
	int maxList = 100;//�������б��С��list���ֵ ����ɾ����ɵ�����
	//���б��β����
	virtual void Push(XData d);
	//��ȡ�б������������ ����������Ҫ����XData.Drop����
	virtual XData Pop();
	//�����߳�
	virtual bool Start();
	//�˳��߳� ���ȴ��߳��˳���������
	virtual void Stop();

	virtual void Clear();

	XDataThread();
	~XDataThread();
protected:
	//��Ž�������  ������ԣ��Ƚ��ȳ�
	std::list<XData> datas;
	//���������б��С
	int dataCount = 0;
	//�������datas
	QMutex mutex;
	//�����߳��˳�
	bool isExit = false;
};

