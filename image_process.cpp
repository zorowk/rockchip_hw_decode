#include <stdio.h>

extern "C" {
#include <libavcodec/avfft.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/pixfmt.h>
#include <libavutil/avstring.h>
#include <libavutil/dict.h>
#include <libavutil/eval.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <libavutil/hwcontext_drm.h>
#include <libdrm/drm_fourcc.h>
}

#define PrintFunLine() printf("%s : %d\n", __FUNCTION__, __LINE__)

int main(int argc, char **argv) {
    // 封装类上下文结构体，统领全局的结构体，保存了视频文件封装格式相关的信息
    AVFormatContext	*pFormatCtx;
    int				i, videoindex;

    // 编码器上下文结构体，保存了视频(音频)对应的结构体
    AVCodecContext	*pCodecCtx;

    // 每种视频（音频）编解码器(例如H.264解码器)对应一个该结构体
    AVCodec			*pCodec;

    // 存储一帧压缩编码数据
    AVPacket *packet;

    // 存储一帧解码后像素（采样）数据。
    AVFrame	*pFrame,*pFrameYUV;
    uint8_t *out_buffer;
    int y_size;
    int ret, got_picture;
    struct SwsContext *img_convert_ctx;
    // 输入文件路径
    char filepath[]="/home/zoro/Documents/test.mp4";

    int frame_cnt;

    // 网络相关
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    // 打开视频文件
    if(avformat_open_input(&pFormatCtx,filepath,nullptr,nullptr)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }

    // 获取视频文件信息
    if(avformat_find_stream_info(pFormatCtx,nullptr)<0){
        printf("Couldn't find stream information.\n");
        return -1;
    }
    // 获取video的角标,方便streams取出视频流
    videoindex=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            break;
        }
    if(videoindex==-1){
        printf("Didn't find a video stream.\n");
        return -1;
    }
    //编写 写入文件  fprintf：打印 写入文件中 记得关闭流
    //FILE *fp = fopen("info.txt","wb+");
    //fprintf(fp,"shichang %d\n",pFormatCtx->streams[videoindex]->codec->width);
    //fclose(fp);

    //streams[] //0：视频  1：音频
    pCodecCtx=pFormatCtx->streams[videoindex]->codec;

    //软解码
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    //硬解码
    //pCodec = avcodec_find_decoder_by_name("h264_rkmpp");
    if(pCodec==nullptr){
        printf("Codec not found.\n");
        return -1;
    }

    // 打开解码器
    if(avcodec_open2(pCodecCtx, pCodec,nullptr)<0){
        printf("Could not open codec.\n");
        return -1;
    }
    /*
     * 在此处添加输出视频信息的代码
     * 取自于pFormatCtx，使用fprintf()
     */
    pFrame=av_frame_alloc();

    pFrameYUV=av_frame_alloc();
    printf("wirdth %d",pFrameYUV->width);
    out_buffer=(uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
    packet=(AVPacket *)av_malloc(sizeof(AVPacket));
    //Output Info-----------------------------
    printf("--------------- File Information ----------------\n");
    av_dump_format(pFormatCtx,0,filepath,0);
    printf("-------------------------------------------------\n");
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);

    frame_cnt=0;
    FILE * fp_frame = fopen("test.yuv","wb+");

    // 从文件逐帧读取数据
    while(av_read_frame(pFormatCtx, packet)>=0){
        if(packet->stream_index==videoindex){
        /*
         * 在此处添加输出H264码流的代码
         * 取自于packet，使用fwrite()
         */
         // fwrite(packet->data,1,packet->size,fp_frame);

            // 解码一帧压缩数据
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if(ret < 0){
                printf("Decode Error.\n");
                return -1;
            }

            if(got_picture){
                sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                    pFrameYUV->data, pFrameYUV->linesize);
                printf("Decoded frame index: %d\n",frame_cnt);

                /*
                 * 在此处添加输出YUV的代码
                 * 取自于pFrameYUV，使用fwrite()
                 */
                fwrite(pFrameYUV->data[0],1,pCodecCtx->height*pCodecCtx->width,fp_frame);
                fwrite(pFrameYUV->data[1],1,pCodecCtx->height*pCodecCtx->width/4,fp_frame);
                fwrite(pFrameYUV->data[2],1,pCodecCtx->height*pCodecCtx->width/4,fp_frame);
                frame_cnt++;

            }
        }
        av_free_packet(packet);
    }
    fclose(fp_frame);
    sws_freeContext(img_convert_ctx);

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);

    // 关闭解码器
    avcodec_close(pCodecCtx);

    // 关闭输入视频文件
    avformat_close_input(&pFormatCtx);
    return 0;
}
