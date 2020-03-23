#include <opencv2/highgui.hpp>
#include<iostream>

#include"XMediaEncode.h"
#include"XRtmp.h"
#pragma comment(lib,"opencv_world320")


using namespace cv;
using namespace std;



int main(int argc, char *argv[]){
	//char *inUrl = "....";//海康相机rtsp
	char *outUrl = "rtmp://192.168.161.128/live";
	namedWindow("video");
	//初始化编码器和像素格式转换对象
	XMediaEncode *me = XMediaEncode::Get(0);
	//创建封装和推流对象
	XRtmp *xr = XRtmp::Get(0);


	
	int ret = 0;
	Mat frame;
	VideoCapture cam;
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	

	
	
	try{
		////1.用opencv打开相机
		cam.open(0);
		if (!cam.isOpened())
		{
			throw exception("cam open failed!");
		}
		cout << "cam open success!" << endl;

		int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);//获取宽度，高度
		int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);
		//int fps = cam.get(CAP_PROP_FPS);
		
		//初始化像素格式转换的上下文
		me->inWidth = inWidth;
		me->inHeight = inHeight;
		me->outWidth = inWidth;
		me->outHeight = inHeight;
		me->InitScale();

		//初始化编码器
		if (!me->InitVideoCodec())
		{
			throw exception("InitVideoCodec failed!");
		}
		//5 输出封装器和视频流配置
		xr->Init(outUrl);
		//添加视频流
		xr->AddStream(me->vc);
		xr->SendHead();
		


		
		for (;;){
			//读取rtsp视频帧，解码视频帧
			if (!cam.grab()){
				continue;
			}
			//yuv转换为rgb
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

			//h264编码
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