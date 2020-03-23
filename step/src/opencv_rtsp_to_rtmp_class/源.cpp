#include <opencv2/highgui.hpp>
#include<iostream>

#include"XMediaEncode.h"
#include"XRtmp.h"
#pragma comment(lib,"opencv_world320")


using namespace cv;
using namespace std;



int main(int argc, char *argv[]){
	//char *inUrl = "....";//�������rtsp
	char *outUrl = "rtmp://192.168.161.128/live";
	namedWindow("video");
	//��ʼ�������������ظ�ʽת������
	XMediaEncode *me = XMediaEncode::Get(0);
	//������װ����������
	XRtmp *xr = XRtmp::Get(0);


	
	int ret = 0;
	Mat frame;
	VideoCapture cam;
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	

	
	
	try{
		////1.��opencv�����
		cam.open(0);
		if (!cam.isOpened())
		{
			throw exception("cam open failed!");
		}
		cout << "cam open success!" << endl;

		int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);//��ȡ��ȣ��߶�
		int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);
		//int fps = cam.get(CAP_PROP_FPS);
		
		//��ʼ�����ظ�ʽת����������
		me->inWidth = inWidth;
		me->inHeight = inHeight;
		me->outWidth = inWidth;
		me->outHeight = inHeight;
		me->InitScale();

		//��ʼ��������
		if (!me->InitVideoCodec())
		{
			throw exception("InitVideoCodec failed!");
		}
		//5 �����װ������Ƶ������
		xr->Init(outUrl);
		//�����Ƶ��
		xr->AddStream(me->vc);
		xr->SendHead();
		


		
		for (;;){
			//��ȡrtsp��Ƶ֡��������Ƶ֡
			if (!cam.grab()){
				continue;
			}
			//yuvת��Ϊrgb
			if (!cam.retrieve(frame)){
				continue;
			}
			imshow("video", frame);
			waitKey(1);
			
	
			//rgb to yuv
			me->inPixSize = frame.elemSize();
			AVFrame *yuv=me->RGBToYUV((char *)frame.data );
			if (!yuv){
				continue;
			}

			//h264����
			if (!me->EncodeVideo(yuv))
			{
				continue;
			}
			AVPacket* pack = me->EncodeVideo(yuv);
			if (!pack)continue;
			

			xr->SendFrame(pack);
		}
	}
	catch (exception &ex){
		if (cam.isOpened()){
			cam.release();
		}
		
		
		cerr << ex.what() << endl;
	}




	getchar();
	return 0;
}