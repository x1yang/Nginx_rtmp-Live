#pragma once

struct AVFrame;
struct AVPacket;
struct AVCodecContext;
//����Ƶ����ӿ���
class XMediaEncode
{
public:
	//�������
	int inWidth = 1280;
	int inHeight = 720;
	int inPixSize = 3;
	//�������
	int outWidth = 1280;
	int outHeight = 720;
	int bitrate = 4000000;//ѹ����ÿ����Ƶ��bit��С 50KB
	int fps = 25;
	//������������
	static XMediaEncode *Get(unsigned char index =0);

	//��ʼ�����ظ�ʽת����������
	virtual bool InitScale()=0;

	virtual AVFrame* RGBToYUV(char *rgb)=0;

	//��������ʼ��
	virtual bool InitVideoCodec()=0;

	//��Ƶ����
	virtual AVPacket *EncodeVideo(AVFrame* frame) = 0;

	virtual ~XMediaEncode();


	AVCodecContext *vc = 0;	//������������
protected:
	XMediaEncode();
};

