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
	
	//1.qt音频开始录制
	QAudioFormat fmt;
	fmt.setSampleRate(sampleRate);
	fmt.setChannelCount(channels);//双声道
	fmt.setSampleSize(sampleByte *8);//通道大小
	fmt.setCodec("audio/pcm");//格式
	fmt.setByteOrder(QAudioFormat::LittleEndian);//字节序
	fmt.setSampleType(QAudioFormat::UnSignedInt);//样本格式
	QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
	if (!info.isFormatSupported(fmt))
	{
		cout << "Audio format not support!" << endl;
		fmt = info.nearestFormat(fmt);
	}

	QAudioInput *input = new QAudioInput(fmt);
	//开始录制音频
	QIODevice *io = input->start();

	///2.音频重采样
	//音频重采样上下文创建，初始化
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


	
	// 4 初始化音频编码器
	if (!xe->InitAudioCode())
	{
		getchar();
		return -1;
	}

	///5.输出封装器和音频流配置
	//a 创建输出封装器上下文
	XRtmp *xr = XRtmp::Get(0);
	if (!xr->Init(outUrl))
	{
		getchar();
		return -1;
	}
	
	//b 添加音频流 
	if (!xr->AddStream(xe->ac))
	{
		getchar();
		return -1;
	}


	//打开rtmp的网络输出io
	
	if (!xr->SendHead())
	{
		getchar();
		return -1;
	}


	//一次读取一帧音频的字节数
	int readSize = xe->nbSample*channels*sampleByte;
	char *buf = new char[readSize];
	
	
	for (;;)
	{
		//一次读取一帧音频	
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
		//已经读一帧源数据
		//重采样源数据
		AVFrame *pcm=xe->Resample(buf);

		//pts 运算
		// nb_sample/smaple_rate  =  一帧音频的秒数
		//  timebase    pts=sec * timebase.den
		AVPacket *pkt=xe->EncodeAudio(pcm);
		if (!pkt)continue;
		//cout << pkt->size << " " << flush;


		////推流
		
		xr->SendFrame(pkt);

		
		
	}
	


	delete buf;

	getchar();



	return a.exec();
}
