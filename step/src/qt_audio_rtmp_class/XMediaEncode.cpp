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
		////��ʼ������������
		//a.�ҵ�������
		if (!CreatCodec(AV_CODEC_ID_H264))
		{
			return false;
		}
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
		return OpenCodec(&vc);
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
	AVPacket *EncodeAudio(AVFrame* frame)
	{
		//pts ����
		// nb_sample/smaple_rate  =  һ֡��Ƶ������
		//  timebase    pts=sec * timebase.den
		pcm->pts = apts;
		apts += av_rescale_q(pcm->nb_samples, { 1,sampleRate }, ac->time_base);

		int ret = avcodec_send_frame(ac, pcm);
		if (ret != 0)return NULL;

		av_packet_unref(&apack);
		ret = avcodec_receive_packet(ac, &apack);//ÿ�ζ����֮ǰ��pkt
		if (ret != 0)return NULL;
		return &apack;
	}
	AVPacket *EncodeVideo(AVFrame* frame)
	{
		av_packet_unref(&vpack);//���ü���-- ������ü���Ϊ0 ���ͷ�
		//h264����
		frame->pts = vpts;
		vpts++;
		int ret = avcodec_send_frame(vc, frame);
		if (ret != 0){
			return NULL;
		}

		ret = avcodec_receive_packet(vc, &vpack);//packÿ�ζ���֮ǰ�����
		if (ret != 0 )//if (ret != 0 || pack.size <= 0)
		{
			
			cout << "*" << vpack.size << flush;
			//return NULL;
		}
		
		return &vpack;
	}
	bool InitResample()
	{
		///2.��Ƶ�ز���
		//��Ƶ�ز��������Ĵ�������ʼ��
		asc = NULL;
		asc = swr_alloc_set_opts(asc,
			av_get_default_channel_layout(channels),(AVSampleFormat) outSampleFmt, sampleRate,//�����ʽ��ͨ������ ��ʽ ������
			av_get_default_channel_layout(channels), (AVSampleFormat)inSampleFmt, sampleRate,//�����ʽ
			0, 0);
		if (!asc)
		{
			cout << "swr_alloc_set_opts failed" << endl;
			return false;
		}
		//�����ĳ�ʼ��
		int ret = swr_init(asc);
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << "*" << endl;
			return false;
		}
		cout << "��Ƶ�ز��������Ĵ�������ʼ���ɹ�" << endl;


		///3.��Ƶ�ز�������ռ����
		pcm = av_frame_alloc();
		pcm->format = outSampleFmt;
		pcm->channels = channels;
		pcm->channel_layout = av_get_default_channel_layout(channels);
		pcm->nb_samples = nbSample;//һ֡��Ƶһͨ���Ĳ�������
		ret = av_frame_get_buffer(pcm, 0);//��pcm����洢�ռ�
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
		int len = swr_convert(asc, pcm->data, pcm->nb_samples,//������� ����洢��ַ����������
			indata, pcm->nb_samples);//�������
		if (len <= 0)
		{
			return NULL;
		}
		return pcm;
	}
private:
	bool OpenCodec(AVCodecContext**c)
	{
		//����Ƶ������
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
		///4.��ʼ����Ƶ������
		//�ҵ� ��ʼ�� AV_CODEC_ID_AAC ������
		AVCodec *codec = avcodec_find_encoder(cid);
		if (!codec)
		{
			cout << "avcodec_find_encoder failed" << endl;
			return false;
		}
		//��Ƶ������������
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
	SwsContext *vsc = NULL;//���ظ�ʽת��������
	AVFrame *yuv = NULL;//��������ݽṹ yuv

	SwrContext *asc = NULL;//��Ƶ�ز���������
	AVFrame *pcm = NULL;//�ز��������pcm

	AVPacket vpack; //��Ƶ֡��0��
	AVPacket apack;//��Ƶ֡	{0}
	int vpts = 0;
	int apts = 0;
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