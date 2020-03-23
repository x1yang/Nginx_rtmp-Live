#include <opencv2/highgui.hpp>
#include<iostream>
#include<string>
using namespace cv;
using namespace std;

#pragma comment(lib,"opencv_world320.lib")
int main(){

	VideoCapture cam;
	//string url = "rtsp://test:test123456@192.168.1.64";//如果使用海康 则cam.open传url

	namedWindow("video");//测试窗口  只用来测试
	Mat frame;
	if (cam.open(0))
	{
		cout << "open cam success!" << endl;
	}
	else{
		cout << "open cam failed!" << endl;
		waitKey(0);
		return -1;
	}

	for (;;){
		cam.read(frame);
		imshow("video", frame);
		waitKey(1);
	}


	getchar();
	return 0;
}
