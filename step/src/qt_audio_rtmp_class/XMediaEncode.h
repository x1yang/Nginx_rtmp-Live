#pragma once

struct AVFrame;
struct AVPacket;
struct AVCodecContext;
enum XSampleFTM
{
	X_S16=1,
	X_FLATP=8
};
//����Ƶ����ӿ���
class XMediaEncode
{
public:
	//�������
	int inWidth = 1280;
	int inHeight = 720;
	int inPixSize = 3;

	int channels = 2;
	int sampleRate = 44100;
	XSampleFTM inSampleFmt = X_S16;
	
	//�������
	int outWidth = 1280;
	int outHeight = 720;
	int bitrate = 4000000;//ѹ����ÿ����Ƶ��bit��С 50KB
	int fps = 25;

	XSampleFTM outSampleFmt = X_FLATP;
	int nbSample = 1024;
	//������������
	static XMediaEncode *Get(unsigned char index =0);

	//��ʼ�����ظ�ʽת����������
	virtual bool InitScale()=0;

	//��Ƶ�ز��������ĳ�ʼ��
	virtual bool InitResample() = 0;

	//����ֵ������������� �ڶ��ε��û�������һ������
	virtual AVFrame* Resample(char*data) = 0;//��Ƶ�ز���

	//����ֵ������������� �ڶ��ε��û�������һ������
	virtual AVFrame* RGBToYUV(char *rgb)=0;

	//��Ƶ��������ʼ��
	virtual bool InitVideoCodec()=0;
	//��Ƶ��������ʼ��
	virtual bool InitAudioCode() = 0;

	//��Ƶ����  ����ֵ������������� �ڶ��ε��û�������һ������
	virtual AVPacket *EncodeVideo(AVFrame* frame) = 0;
	//��Ƶ����  ����ֵ������������� �ڶ��ε��û�������һ������
	virtual AVPacket *EncodeAudio(AVFrame* frame) = 0;
	virtual ~XMediaEncode();


	AVCodecContext *vc = 0;	//��Ƶ������������
	AVCodecContext *ac = 0; //��Ƶ������������
protected:
	XMediaEncode();
};

