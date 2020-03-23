#include <QtCore/QCoreApplication>

#include<iostream>
#include<QThread>
#include"XMediaEncode.h"
#include"XRtmp.h"
#include"XAudioRecord.h"
#include"XVideoCapture.h"
#include"XFilter.h"
#include"XController.h"
using namespace std;
int main(int argc, char *argv[])
{
	
	QCoreApplication a(argc, argv);

	const char *outUrl = "rtmp://192.168.161.128/live";
	XController::Get()->Stop();
	XController::Get()->camIndex = 0;
	XController::Get()->outUrl = outUrl;
	XController::Get()->Start();
	long long beginTime = GetCurTime();//开始计时当前时间
	XController::Get()->wait();

	return a.exec();
}
