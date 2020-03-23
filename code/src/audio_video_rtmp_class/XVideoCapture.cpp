#include "XVideoCapture.h"
#include <opencv2/highgui.hpp>
#include<iostream>
#pragma comment(lib,"opencv_world320")
using namespace cv;
using namespace std;
class CXVideoCapture :public XVideoCapture
{
public:
	VideoCapture cam;
	void run()
	{
		Mat frame;
		while (!isExit)
		{
			if (!cam.read(frame))
			{
				msleep(1);
				continue;
			}
			
			if (frame.empty())
			{
				msleep(1);
				continue;
			}
			//确保数据是连续的 
			fmutex.lock();
			for (int i = 0; i < filters.size(); i++)
			{
				Mat des;
				filters[i]->Filter(&frame,&des);
				frame = des;
			}

			fmutex.unlock();

			XData d((char *)frame.data, frame.cols*frame.rows*frame.elemSize(),GetCurTime());
			Push(d);
			
		}
	}

	virtual bool Init(int camIndex = 0)
	{
		cam.open(camIndex);
		if (!cam.isOpened())
		{
			cout<<"cam open failed!"<<endl;
			return false;
		}
		cout << camIndex<< "cam open success!" << endl;

		 width = cam.get(CAP_PROP_FRAME_WIDTH);//获取宽度，高度
		 height = cam.get(CAP_PROP_FRAME_HEIGHT);
		 fps = cam.get(CAP_PROP_FPS);//7.3开头
		 if (fps == 0) fps = 25;
		return true;
	}
	virtual bool Init(const char*url)
	{
		cam.open(url);
		if (!cam.isOpened())
		{
			cout << "cam open failed!" << endl;
			return false;
		}
		cout << url << "cam open success!" << endl;

		width = cam.get(CAP_PROP_FRAME_WIDTH);//获取宽度，高度
		height = cam.get(CAP_PROP_FRAME_HEIGHT);
		 fps = cam.get(CAP_PROP_FPS);//7.3开头
		if (fps == 0) fps = 25;
		return true;
	}
	void Stop()
	{
		XDataThread::Stop();
		if (cam.isOpened())
		{
			cam.release();
		}
	}

};
XVideoCapture *XVideoCapture::Get(unsigned char index)
{
	static CXVideoCapture xc[255];
	return &xc[index];
}

XVideoCapture::XVideoCapture()
{
}


XVideoCapture::~XVideoCapture()
{
}
