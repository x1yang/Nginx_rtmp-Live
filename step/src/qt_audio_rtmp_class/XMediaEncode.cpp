#include "XMediaEncode.h"
extern"C"{
#include<libswscale/swscale.h>
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libswresample/swresample.h>
}
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swresample.lib")
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
		if (pcm) {
			av_frame_free(&pcm);
		}
		if (asc)
		{
			swr_free(&asc);
		}
		vpts = 0;
		apts = 0;
		av_packet_unref(&vpack);
		av_packet_unref(&apack);
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
			cout << "av_frame_get_buffer failed" << endl;
		}


		return true;
	}
	bool InitAudioCode()
	{
		if (!CreatCodec(AV_CODEC_ID_AAC))
		{
			return false;
		}
		ac->bit_rate = 40000;
		ac->sample_rate = sampleRate;
		ac->sample_fmt = AV_SAMPLE_FMT_FLTP;//fltp
		ac->channels = channels;
		ac->channel_layout = av_get_default_channel_layout(channels);

		return OpenCodec(&ac);
	}
	bool InitVideoCodec()
	{
		////初始化编码上下文
		//a.找到编码器
		if (!CreatCodec(AV_CODEC_ID_H264))
		{
			return false;
		}
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
		return OpenCodec(&vc);
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
	AVPacket *EncodeAudio(AVFrame* frame)
	{
		//pts 运算
		// nb_sample/smaple_rate  =  一帧音频的秒数
		//  timebase    pts=sec * timebase.den
		pcm->pts = apts;
		apts += av_rescale_q(pcm->nb_samples, { 1,sampleRate }, ac->time_base);

		int ret = avcodec_send_frame(ac, pcm);
		if (ret != 0)return NULL;

		av_packet_unref(&apack);
		ret = avcodec_receive_packet(ac, &apack);//每次都清掉之前的pkt
		if (ret != 0)return NULL;
		return &apack;
	}
	AVPacket *EncodeVideo(AVFrame* frame)
	{
		av_packet_unref(&vpack);//引用计数-- 如果引用计数为0 则释放
		//h264编码
		frame->pts = vpts;
		vpts++;
		int ret = avcodec_send_frame(vc, frame);
		if (ret != 0){
			return NULL;
		}

		ret = avcodec_receive_packet(vc, &vpack);//pack每次都把之前的清掉
		if (ret != 0 )//if (ret != 0 || pack.size <= 0)
		{
			
			cout << "*" << vpack.size << flush;
			//return NULL;
		}
		
		return &vpack;
	}
	bool InitResample()
	{
		///2.音频重采样
		//音频重采样上下文创建，初始化
		asc = NULL;
		asc = swr_alloc_set_opts(asc,
			av_get_default_channel_layout(channels),(AVSampleFormat) outSampleFmt, sampleRate,//输出格式：通道类型 格式 样本率
			av_get_default_channel_layout(channels), (AVSampleFormat)inSampleFmt, sampleRate,//输入格式
			0, 0);
		if (!asc)
		{
			cout << "swr_alloc_set_opts failed" << endl;
			return false;
		}
		//上下文初始化
		int ret = swr_init(asc);
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << "*" << endl;
			return false;
		}
		cout << "音频重采样上下文创建，初始化成功" << endl;


		///3.音频重采样输出空间分配
		pcm = av_frame_alloc();
		pcm->format = outSampleFmt;
		pcm->channels = channels;
		pcm->channel_layout = av_get_default_channel_layout(channels);
		pcm->nb_samples = nbSample;//一帧音频一通道的采样数量
		ret = av_frame_get_buffer(pcm, 0);//给pcm分配存储空间
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << "*" << endl;
			return false;
		}
		return true;
	}
	AVFrame* Resample(char*data)
	{
		const uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = (uint8_t*)data;
		int len = swr_convert(asc, pcm->data, pcm->nb_samples,//输出参数 输出存储地址和样本数量
			indata, pcm->nb_samples);//输入参数
		if (len <= 0)
		{
			return NULL;
		}
		return pcm;
	}
private:
	bool OpenCodec(AVCodecContext**c)
	{
		//打开音频编码器
		int ret = avcodec_open2(*c, 0, 0);
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << "*" << endl;
			avcodec_free_context(c);
			return false;
		}
		cout << "avcodec_open2 success!" << endl;
		return true;
	}
	bool CreatCodec(AVCodecID cid)
	{
		///4.初始化音频编码器
		//找到 初始化 AV_CODEC_ID_AAC 编码器
		AVCodec *codec = avcodec_find_encoder(cid);
		if (!codec)
		{
			cout << "avcodec_find_encoder failed" << endl;
			return false;
		}
		//音频编码器上下文
		ac = avcodec_alloc_context3(codec);
		if (!codec)
		{
			cout << "avcodec_alloc_context3 failed" << endl;
			return false;
		}
		cout << "avcodec_alloc_context3 success!" << endl;

		ac->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		ac->thread_count = XGetCpuNum();
		return true;
	}
	SwsContext *vsc = NULL;//像素格式转换上下文
	AVFrame *yuv = NULL;//输出的数据结构 yuv

	SwrContext *asc = NULL;//音频重采样上下文
	AVFrame *pcm = NULL;//重采样输出的pcm

	AVPacket vpack; //视频帧｛0｝
	AVPacket apack;//音频帧	{0}
	int vpts = 0;
	int apts = 0;
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
