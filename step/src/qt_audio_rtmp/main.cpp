#include <QtCore/QCoreApplication>
#include<QAudioInput>
#include<iostream>
#include<QThread>
extern"C"
{
#include <libavcodec/avcodec.h>
#include<libswresample/swresample.h>
#include<libavformat/avformat.h>
}
#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
using namespace std;
int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	char *outUrl = "rtmp://192.168.161.128/live";
	avcodec_register_all();//ע�����еı������
	av_register_all();//ע�����еķ�װ��
	avformat_network_init();//ע�����е�����Э��

	int sampleRate = 44100;
	int channels = 2;
	int sampleByte = 2;
	AVSampleFormat inSampleFmt = AV_SAMPLE_FMT_S16;
	AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_FLTP;
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
	SwrContext *asc = NULL;
	asc = swr_alloc_set_opts(asc,
		av_get_default_channel_layout(channels), outSampleFmt,sampleRate,//�����ʽ��ͨ������ ��ʽ ������
		av_get_default_channel_layout(channels), inSampleFmt, sampleRate,//�����ʽ
		0,0);
	if (!asc)
	{
		cout << "swr_alloc_set_opts failed"<<endl;
		getchar();
		return -1;
	}
	//�����ĳ�ʼ��
	int ret = swr_init(asc);
	if (ret!=0)
	{
		char err[1024] = { 0 };
		av_strerror(ret,err,sizeof(err)-1);
		cout << err <<"*"<< endl;
		getchar();
		return -1;
	}
	cout << "��Ƶ�ز��������Ĵ�������ʼ���ɹ�"<< endl;


	///3.��Ƶ�ز�������ռ����
	AVFrame *pcm = av_frame_alloc();
	pcm->format= outSampleFmt;
	pcm->channels = channels;
	pcm->channel_layout = av_get_default_channel_layout(channels);
	pcm->nb_samples = 1024;//һ֡��Ƶһͨ���Ĳ�������
	ret=av_frame_get_buffer(pcm,0);//��pcm����洢�ռ�
	if (ret != 0)
	{
		char err[1024] = { 0 };
		av_strerror(ret, err, sizeof(err) - 1);
		cout << err << "*" << endl;
		getchar();
		return -1;
	}
	///4.��ʼ����Ƶ������
	//�ҵ� AV_CODEC_ID_AAC ������
	AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec)
	{
		cout << "avcodec_find_encoder failed" << endl;
		getchar();
		return -1;
	}
	//��Ƶ������������
	AVCodecContext *ac = avcodec_alloc_context3(codec);
	if (!codec)
	{
		cout << "avcodec_alloc_context3 failed" << endl;
		getchar();
		return -1;
	}
	cout << "avcodec_alloc_context3 success!" << endl;

	ac->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	ac->thread_count = 8;
	ac->bit_rate = 40000;
	ac->sample_rate = sampleRate;
	ac->sample_fmt = outSampleFmt;//fltp
	ac->channels = channels;
	ac->channel_layout = av_get_default_channel_layout(channels);
	


	//����Ƶ������
	ret = avcodec_open2(ac,0,0);
	if (ret != 0)
	{
		char err[1024] = { 0 };
		av_strerror(ret, err, sizeof(err) - 1);
		cout << err << "*" << endl;
		getchar();
		return -1;
	}
	cout << "avcodec_open2 success!" << endl;


	///5.�����װ������Ƶ������
	//a ���������װ��������
	AVFormatContext *ic = NULL;
	ret = avformat_alloc_output_context2(&ic, 0, "flv", outUrl);
	if (ret != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		cout << buf << endl;
		cout << "alloc" << endl;
		getchar();
		return -1;
	}
	//b �����Ƶ�� 
	AVStream *as = avformat_new_stream(ic, NULL);
	if (!as)
	{
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		cout << buf << endl;
		cout << "avstream" << endl;
		getchar();
		return -1;
	}
	as->codecpar->codec_tag = 0;
	//�ӱ��������Ʋ���
	avcodec_parameters_from_context(as->codecpar, ac);
	av_dump_format(ic, 0, outUrl, 1);


	//��rtmp���������io
	ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
	if (ret != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		cout << buf << endl;
		cout << "open" << endl;
		getchar();
		return -1;
	}
	//д���װͷ
	ret = avformat_write_header(ic, NULL);
	if (ret != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		cout << buf << endl;
		cout << "write header" << endl;
		getchar();
		return -1;
	}



	//һ�ζ�ȡһ֡��Ƶ���ֽ���
	int readSize = pcm->nb_samples*channels*sampleByte;
	char *buf = new char[readSize];
	AVPacket pkt ;
	memset(&pkt, 0, sizeof(pkt));
	int apts = 0;
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
		const uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = (uint8_t*)buf;
		int len=swr_convert(asc,pcm->data,pcm->nb_samples,//������� ����洢��ַ����������
			indata,pcm->nb_samples);//�������


		//pts ����
		// nb_sample/smaple_rate  =  һ֡��Ƶ������
		//  timebase    pts=sec * timebase.den
		pcm->pts = apts;
		apts += av_rescale_q(pcm->nb_samples, {1,sampleRate},ac->time_base);

		int ret = avcodec_send_frame(ac,pcm);
		if (ret != 0)continue;
		
			av_packet_unref(&pkt);
		ret=avcodec_receive_packet(ac,&pkt);//ÿ�ζ����֮ǰ��pkt
		if (ret != 0)continue;
		
		cout << pkt.size << " " << flush;


		//����
		
		pkt.pts = av_rescale_q(pkt.pts, ac->time_base, as->time_base);
		pkt.dts = av_rescale_q(pkt.dts, ac->time_base, as->time_base);
		
		pkt.duration = av_rescale_q(pkt.duration,ac->time_base,as->time_base);
		
		ret = av_interleaved_write_frame(ic, &pkt);//�˺�����������
		if (ret == 0)
		{
			cout << "#" << flush;
		}
		
	}
	


	delete buf;

	getchar();



	return a.exec();
}
