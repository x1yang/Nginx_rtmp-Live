#include <QtCore/QCoreApplication>
#include<QAudioInput>
#include<iostream>
#include<QThread>
#include"XMediaEncode.h"
#include"XRtmp.h"

using namespace std;
int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	char *outUrl = "rtmp://192.168.161.128/live";
	
	int sampleRate = 44100;
	int channels = 2;
	int sampleByte = 2;
	
	//1.qt��Ƶ��ʼ¼��
	QAudioFormat fmt;
	fmt.setSampleRate(sampleRate);
	fmt.setChannelCount(channels);//˫����
	fmt.setSampleSize(sampleByte *8);//ͨ����С
	fmt.setCodec("audio/pcm");//��ʽ
	fmt.setByteOrder(QAudioFormat::LittleEndian);//�ֽ���
	fmt.setSampleType(QAudioFormat::UnSignedInt);//������ʽ
	QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
	if (!info.isFormatSupported(fmt))
	{
		cout << "Audio format not support!" << endl;
		fmt = info.nearestFormat(fmt);
	}

	QAudioInput *input = new QAudioInput(fmt);
	//��ʼ¼����Ƶ
	QIODevice *io = input->start();

	///2.��Ƶ�ز���
	//��Ƶ�ز��������Ĵ�������ʼ��
	XMediaEncode*xe=XMediaEncode::Get();
	xe->channels = channels;
	xe->nbSample = 1024;
	xe->sampleRate = sampleRate;
	xe->inSampleFmt = XSampleFTM::X_S16;
	xe->outSampleFmt = XSampleFTM::X_FLATP;
	if (!xe->InitResample())
	{
		getchar();
		return -1;
	}


	
	// 4 ��ʼ����Ƶ������
	if (!xe->InitAudioCode())
	{
		getchar();
		return -1;
	}

	///5.�����װ������Ƶ������
	//a ���������װ��������
	XRtmp *xr = XRtmp::Get(0);
	if (!xr->Init(outUrl))
	{
		getchar();
		return -1;
	}
	
	//b �����Ƶ�� 
	if (!xr->AddStream(xe->ac))
	{
		getchar();
		return -1;
	}


	//��rtmp���������io
	
	if (!xr->SendHead())
	{
		getchar();
		return -1;
	}


	//һ�ζ�ȡһ֡��Ƶ���ֽ���
	int readSize = xe->nbSample*channels*sampleByte;
	char *buf = new char[readSize];
	
	
	for (;;)
	{
		//һ�ζ�ȡһ֡��Ƶ	
		if (input->bytesReady() < readSize)
		{
			QThread::msleep(1);
			continue;
		}
		int size = 0;
		while (size != readSize)
		{
			int len= io->read(buf+size, readSize-size);
			if (len < 0) break;
			size += len;
		}
		if (size != readSize) continue;
		//�Ѿ���һ֡Դ����
		//�ز���Դ����
		AVFrame *pcm=xe->Resample(buf);

		//pts ����
		// nb_sample/smaple_rate  =  һ֡��Ƶ������
		//  timebase    pts=sec * timebase.den
		AVPacket *pkt=xe->EncodeAudio(pcm);
		if (!pkt)continue;
		//cout << pkt->size << " " << flush;


		////����
		
		xr->SendFrame(pkt);

		
		
	}
	


	delete buf;

	getchar();



	return a.exec();
}
