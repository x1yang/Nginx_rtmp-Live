#include <opencv2/highgui.hpp>
#include<iostream>
extern"C"{
#include<libswscale/swscale.h>
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
}
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"opencv_world320")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
using namespace cv;
using namespace std;



int main(int argc, char *argv[]){
	//char *inUrl = "....";//�������rtsp
	char *outUrl = "rtmp://192.168.161.128/live";
	namedWindow("video");
	avcodec_register_all();//ע�����еı������
	av_register_all();//ע�����еķ�װ��
	avformat_network_init();//ע�����е�����Э��
	Mat frame;
	VideoCapture cam;
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//���ظ�ʽת��������
	SwsContext *vsc = NULL;
	//��������ݽṹ
	AVFrame *yuv = NULL;
	//������������
	AVCodecContext *vc = NULL;
	//rtmp flv ��װ��
	AVFormatContext *ic = NULL;
	try{
		////1.��opencv�����
		cam.open(0);
		cout << "123" << endl;
		if (!cam.isOpened())
		{
			throw exception("cam open failed!");
		}
		cout << "cam open success!" << endl;
		////2 ��ʼ����ʽת��������

		int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);//��ȡ��ȣ��߶�
		int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);
		//int fps = cam.get(CAP_PROP_FPS);
		
		//������ʽ�����ģ�ת��bgr-yuv��  �ߴ�仯ʹ��SWS_BICUBIC�㷨 
		vsc = sws_getCachedContext(vsc, inWidth, inHeight, AV_PIX_FMT_BGR24,
			inWidth, inHeight, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
		if (!vsc){
			throw exception("sws_getCachedContext failed!");
		}
		////3.��ʼ����������ݽṹ
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
		////4.��ʼ������������
		//a.�ҵ�������
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!codec){
			throw exception("cant find h264 encoder!");
		}
		//b.����������������
		vc = avcodec_alloc_context3(codec);
		if (!vc){
			throw exception("avcodec_alloc_context3 failed!");
		}
		//c.���ñ���������
		vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//ȫ�ֲ���
		vc->codec_id = codec->id;
		vc->thread_count = 8;
		//������Щֻ����Ƶ�����е�
		vc->bit_rate = 50*1024*8;//ѹ����ÿ����Ƶ��λ��С  50kB  ѹ����
		vc->width = inWidth;
		vc->height = inHeight;
		vc->time_base = { 1, 20 };//��λ����   //20Ӧ��Ϊcam.fps
		vc->framerate = { 20, 1 };//֡��
		//������Ĵ�С������֡һ���ؼ�֡
		vc->gop_size = 50;
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;
		//d.�򿪱�����������
		ret = avcodec_open2(vc,0,0);
		if (ret!=0){
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf)-1);
			throw exception(buf);
		}
		cout << "avcodec_open2 success!" << endl;

		//5 �����װ������Ƶ������
		//a ���������װ��������
		ret=avformat_alloc_output_context2(&ic,0,"flv",outUrl);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf)-1);
			throw exception(buf);
		}
		//b �����Ƶ�� 
		AVStream *vs=avformat_new_stream(ic,NULL);
		if (!vs)
		{
			throw exception("avformat_new_stream failed");
		}
		vs->codecpar->codec_tag = 0;
		//�ӱ��������Ʋ���
		avcodec_parameters_from_context(vs->codecpar,vc);
		av_dump_format(ic,0,outUrl,1);

		//��rtmp���������io
		ret = avio_open(&ic->pb,outUrl,AVIO_FLAG_WRITE);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf)-1);
			throw exception(buf);
		}
		//д���װͷ
		ret = avformat_write_header(ic,NULL);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf)-1);
			throw exception(buf);
		}


		AVPacket pack;
		memset(&pack,0,sizeof(pack));
		int vpts = 0;
		for (;;){
			//��ȡrtsp��Ƶ֡��������Ƶ֡
			if (!cam.grab()){
				continue;
			}
			//yuvת��Ϊrgb
			if (!cam.retrieve(frame)){
				continue;
			}
			imshow("video", frame);
			waitKey(1);
			//RGB TO YUV
			//��������ݽṹ
			uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
			//bgrbgrbgr
			//���plane��ŷ�ʽ      indata[0]bbbbb indata[1]ggggg indata[2]rrrrr
			indata[0] = frame.data;
			int insize[AV_NUM_DATA_POINTERS] = { 0 };
			//һ�����ݵ��ֽ���(��)
			insize[0] = frame.elemSize()*frame.cols;
			int h = sws_scale(vsc, indata, insize, 0, frame.rows,//Դ����
				yuv->data, yuv->linesize);
			if (h <= 0){
				continue;
			}
			cout << h << "  " << flush;
			//h264����
			yuv->pts = vpts;
			vpts++;
			ret=avcodec_send_frame(vc,yuv);
			if (ret!=0){
				continue;
			}

			ret=avcodec_receive_packet(vc, &pack);//packÿ�ζ���֮ǰ�����
			if (ret!=0||pack.size>0){
				cout <<"*"<<pack.size << flush;
			}
			else{
				continue;
			}
			//����

			pack.pts = av_rescale_q(pack.pts,vc->time_base,vs->time_base);
			pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
			ret=av_interleaved_write_frame(ic,&pack);
			if (ret == 0)
			{	
				cout << "#" << endl;
			}
		}
	}
	catch (exception &ex){
		if (cam.isOpened()){
			cam.release();
		}
		if (vsc){
			sws_freeContext(vsc);
			vsc = NULL;
		}
		if (vc){
			avio_closep(&ic->pb);
			avcodec_free_context(&vc);

		}
		cerr << ex.what() << endl;
	}




	getchar();
	return 0;
}