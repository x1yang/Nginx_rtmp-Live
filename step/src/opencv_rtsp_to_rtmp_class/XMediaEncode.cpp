#include "XMediaEncode.h"
extern"C"{
#include<libswscale/swscale.h>
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
}
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#include <iostream>
using namespace std;


//��ȡCPU����
#if defined WIN32||defined _WIN32
#include <windows.h>
#endif
static int XGetCpuNum()
{
#if defined WIN32||defined _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	return (int)sysinfo.dwNumberOfProcessors;
#elif defined _linux_
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined _APPLE_
	int numCPU = 0;
	int mib[4];
	size_t len = sizeof(numCPU);
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;
	sysctl(mib,2,&numCPU,&len,NULL,0);
	if (numCPU<1){
		mib[1] = HW_NCPU;
		sysctl(mib,2,&numCPU,&len,NULL,0);
		if (numCPU < 1)
			numCPU = 1;
	}
	return (int)numCPU;
#else
	return 1;
#endif
}


class CXMediatEncode :public XMediaEncode
{
public:
	void Close()
	{
		if (vsc){
			sws_freeContext(vsc);
			vsc = NULL;
		}
		if (yuv){
			av_frame_free(&yuv);
		}
		if (vc){
			avcodec_free_context(&vc);
		}
		vpts = 0;
		av_packet_unref(&pack);
	}
	bool InitScale()
	{
		//������ʽ�����ģ�ת��bgr-yuv��  �ߴ�仯ʹ��SWS_BICUBIC�㷨 
		vsc = sws_getCachedContext(vsc, inWidth, inHeight, AV_PIX_FMT_BGR24,
			outWidth, outHeight, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
		if (!vsc){
			cout<<"sws_getCachedContext failed!";
			return false;
		}
		////��ʼ����������ݽṹ
		yuv = av_frame_alloc();
		yuv->format = AV_PIX_FMT_YUV420P;
		yuv->width = inWidth;
		yuv->height = inHeight;
		yuv->pts = 0;
		//����yuv�ռ�
		int ret = av_frame_get_buffer(yuv, 32);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf)-1);
			throw exception(buf);
		}


		return true;
	}
	bool InitVideoCodec()
	{
		////��ʼ������������
		//a.�ҵ�������
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!codec){
			cout<<"cant find h264 encoder!"<<endl;
			return false;
		}
		//b.����������������
		vc = avcodec_alloc_context3(codec);
		if (!vc){
			cout << "avcodec_alloc_context3 failed!" << endl;
		}
		//c.���ñ���������
		vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//ȫ�ֲ���
		vc->codec_id = codec->id;
		vc->thread_count = 8;
		//������Щֻ����Ƶ�����е�
		vc->bit_rate = 50 * 1024 * 8;//ѹ����ÿ����Ƶ��λ��С  50kB  ѹ����
		vc->width = outWidth;
		vc->height = outHeight;
		vc->time_base = { 1, fps };//��λ����   //20Ӧ��Ϊcam.fps
		vc->framerate = { fps, 1 };//֡��
		//������Ĵ�С������֡һ���ؼ�֡
		vc->gop_size = 50;
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;
		//d.�򿪱�����������
		int	ret = avcodec_open2(vc, 0, 0);
		if (ret != 0){
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf)-1);
			cout << buf<<endl;
			return false;
		}
		cout << "avcodec_open2 success!" << endl;
		return true;
	}
	AVFrame* RGBToYUV(char *rgb)
	{
		//RGB TO YUV
		//��������ݽṹ
		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		//bgrbgrbgr
		//���plane��ŷ�ʽ      indata[0]bbbbb indata[1]ggggg indata[2]rrrrr
		indata[0] = (uint8_t*)rgb;
		int insize[AV_NUM_DATA_POINTERS] = { 0 };
		//һ�����ݵ��ֽ���(��)
		insize[0] =inWidth*inPixSize;
		int h = sws_scale(vsc, indata, insize, 0, inHeight,//Դ����
			yuv->data, yuv->linesize);
		if (h <= 0){
			return NULL;
		}
		return yuv;
	}
	AVPacket *EncodeVideo(AVFrame* frame)
	{
		av_packet_unref(&pack);//���ü���-- ������ü���Ϊ0 ���ͷ�
		//h264����
		frame->pts = vpts;
		vpts++;
		int ret = avcodec_send_frame(vc, frame);
		if (ret != 0){
			return NULL;
		}

		ret = avcodec_receive_packet(vc, &pack);//packÿ�ζ���֮ǰ�����
		if (ret != 0 )//if (ret != 0 || pack.size <= 0)
		{
			
			cout << "*" << pack.size << flush;
			//return NULL;
		}
		
		return &pack;
	}
private:
	SwsContext *vsc = NULL;//���ظ�ʽת��������
	AVFrame *yuv = NULL;//��������ݽṹ yuv
	AVPacket pack ;
	
	int vpts = 0;
};

XMediaEncode* XMediaEncode::Get(unsigned char index)
{
	static bool isFirst = true;
	if (isFirst)
	{
		avcodec_register_all();//ע�����еı������
		isFirst = false;
	}
	static CXMediatEncode cxm[255];
	return &cxm[index];
}
XMediaEncode::XMediaEncode()
{
}


XMediaEncode::~XMediaEncode()
{
}
