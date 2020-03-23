#include <iostream>
extern "C"
{
#include"libavformat/avformat.h"
}
using namespace std;
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
int xError(int errNum)
{
	char buf[1024] = { 0 };
	av_strerror(errNum, buf, sizeof(buf));
	cout << buf << endl;
	getchar();
	return -1;
}
int main(int argc,char*argv[])
{
	av_register_all();//初始化所有封装 解封装
	avformat_network_init();//初始化网络库

	//打开文件 解封装
	//输入封装上下文
	AVFormatContext *ictx = NULL;

	char *inUrl = "test.mp4";

	int re = avformat_open_input(&ictx, inUrl, 0, 0);
	if (re!=0){
		return xError(re);
	}
	cout << "avformat_open_input success" << endl;
	//获取音视频流信息
	re=avformat_find_stream_info(ictx,0);
	if (re != 0){
		return xError(re);
	}
	av_dump_format(ictx, 0, inUrl, 0);
	getchar();
	return 0;

}