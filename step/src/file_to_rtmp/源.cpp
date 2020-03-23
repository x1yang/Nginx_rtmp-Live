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
	av_register_all();//��ʼ�����з�װ ���װ
	avformat_network_init();//��ʼ�������

	//���ļ� ���װ
	//�����װ������
	AVFormatContext *ictx = NULL;

	char *inUrl = "test.mp4";

	int re = avformat_open_input(&ictx, inUrl, 0, 0);
	if (re!=0){
		return xError(re);
	}
	cout << "avformat_open_input success" << endl;
	//��ȡ����Ƶ����Ϣ
	re=avformat_find_stream_info(ictx,0);
	if (re != 0){
		return xError(re);
	}
	av_dump_format(ictx, 0, inUrl, 0);
	getchar();
	return 0;

}