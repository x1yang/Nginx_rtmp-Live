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


//获取CPU数量
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
		//创建格式上下文（转换bgr-yuv）  尺寸变化使用SWS_BICUBIC算法 
		vsc = sws_getCachedContext(vsc, inWidth, inHeight, AV_PIX_FMT_BGR24,
			outWidth, outHeight, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
		if (!vsc){
			cout<<"sws_getCachedContext failed!";
			return false;
		}
		////初始化输出的数据结构
		yuv = av_frame_alloc();
		yuv->format = AV_PIX_FMT_YUV420P;
		yuv->width = inWidth;
		yuv->height = inHeight;
		yuv->pts = 0;
		//分配yuv空间
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
		////初始化编码上下文
		//a.找到编码器
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!codec){
			cout<<"cant find h264 encoder!"<<endl;
			return false;
		}
		//b.创建编码器上下文
		vc = avcodec_alloc_context3(codec);
		if (!vc){
			cout << "avcodec_alloc_context3 failed!" << endl;
		}
		//c.配置编码器参数
		vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//全局参数
		vc->codec_id = codec->id;
		vc->thread_count = 8;
		//下面这些只有视频编码有的
		vc->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的位大小  50kB  压缩率
		vc->width = outWidth;
		vc->height = outHeight;
		vc->time_base = { 1, fps };//单位是秒   //20应该为cam.fps
		vc->framerate = { fps, 1 };//帧率
		//画面组的大小，多少帧一个关键帧
		vc->gop_size = 50;
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;
		//d.打开编码器上下文
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
		//输入的数据结构
		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		//bgrbgrbgr
		//如果plane存放方式      indata[0]bbbbb indata[1]ggggg indata[2]rrrrr
		indata[0] = (uint8_t*)rgb;
		int insize[AV_NUM_DATA_POINTERS] = { 0 };
		//一行数据的字节数(宽)
		insize[0] =inWidth*inPixSize;
		int h = sws_scale(vsc, indata, insize, 0, inHeight,//源数据
			yuv->data, yuv->linesize);
		if (h <= 0){
			return NULL;
		}
		return yuv;
	}
	AVPacket *EncodeVideo(AVFrame* frame)
	{
		av_packet_unref(&pack);//引用计数-- 如果引用计数为0 则释放
		//h264编码
		frame->pts = vpts;
		vpts++;
		int ret = avcodec_send_frame(vc, frame);
		if (ret != 0){
			return NULL;
		}

		ret = avcodec_receive_packet(vc, &pack);//pack每次都把之前的清掉
		if (ret != 0 )//if (ret != 0 || pack.size <= 0)
		{
			
			cout << "*" << pack.size << flush;
			//return NULL;
		}
		
		return &pack;
	}
private:
	SwsContext *vsc = NULL;//像素格式转换上下文
	AVFrame *yuv = NULL;//输出的数据结构 yuv
	AVPacket pack ;
	
	int vpts = 0;
};

XMediaEncode* XMediaEncode::Get(unsigned char index)
{
	static bool isFirst = true;
	if (isFirst)
	{
		avcodec_register_all();//注册所有的编解码器
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
