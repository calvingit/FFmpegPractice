#include <cstdio>

//ffmpeg 1.0
#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#ifdef __cplusplus
}
#endif
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

//sdl2
#include "sdl.h" 
#pragma comment(lib, "SDL2.lib")

int main(int argc, char* argv[])
{
	AVFormatContext    *pFormatCtx;
	int                i, videoindex;
	AVCodecContext    *pCodecCtx;
	AVCodec            *pCodec;

	av_register_all();
	avcodec_register_all();
	avformat_network_init();

	pFormatCtx = avformat_alloc_context();
	if(avformat_open_input(&pFormatCtx,argv[1],NULL,NULL)!=0){
		printf("无法打开文件\n");
		return -1;
	}
	if(av_find_stream_info(pFormatCtx)<0)
	{
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoindex=i;
			break;
		}
		if(videoindex==-1)
		{
			printf("Didn't find a video stream.\n");
			return -1;
		}
		pCodecCtx=pFormatCtx->streams[videoindex]->codec;
		pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
		if(pCodec==NULL)
		{
			printf("Codec not found.\n");
			return -1;
		}
		if(avcodec_open(pCodecCtx, pCodec)<0)
		{
			printf("Could not open codec.\n");
			return -1;
		}
		AVFrame    *pFrame,*pFrameYUV;
		pFrame=avcodec_alloc_frame();
		pFrameYUV=avcodec_alloc_frame();
		uint8_t *out_buffer;
		out_buffer=new uint8_t[avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height)];
		avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
		//------------SDL----------------
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
			printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
			exit(1);
		} 
		/*
		SDL_Surface *screen; 
		screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
		if(!screen) {  printf("SDL: could not set video mode - exiting\n");  
		exit(1);
		}
		SDL_Overlay *bmp; 
		bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,SDL_YV12_OVERLAY, screen); 
		SDL_Rect rect;*/
		
		SDL_Window *screen = SDL_CreateWindow("Play", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
			pCodecCtx->width, pCodecCtx->height, SDL_WINDOW_SHOWN);
		if (screen == NULL)
		{
			printf("SDL_CreateWindow failed\n");
			exit(1);
		}

		SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);
		if (renderer == NULL)
		{
			printf("SDL_CreateRenderer failed\n");
			exit(1);
		}

		//clear the screen to black
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
		SDL_RenderSetLogicalSize(renderer, pCodecCtx->width, pCodecCtx->height);

		SDL_Texture *sdlTexture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_YV12,
			SDL_TEXTUREACCESS_STREAMING,
			pCodecCtx->width, pCodecCtx->height);
		if (sdlTexture == NULL)
		{
			printf("SDL_CreateTexture failed\n");
			exit(0);
		}
		uint8_t *pixels1 = NULL;
		int pitch1;
		//---------------
		int ret, got_picture;
		static struct SwsContext *img_convert_ctx;
		int y_size = pCodecCtx->width * pCodecCtx->height;

		AVPacket *packet=(AVPacket *)malloc(sizeof(AVPacket));
		av_new_packet(packet, y_size);
		//输出一下信息-----------------------------
		printf("文件信息-----------------------------------------\n");
		av_dump_format(pFormatCtx,0,argv[1],0);
		printf("-------------------------------------------------\n");
		//------------------------------
		while(av_read_frame(pFormatCtx, packet)>=0)
		{
			if(packet->stream_index==videoindex)
			{
				ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
				if(ret < 0)
				{
					printf("解码错误\n");
					return -1;
				}
				if(got_picture)
				{
					img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
						pCodecCtx->pix_fmt,
						pCodecCtx->width, pCodecCtx->height, 
						PIX_FMT_YUV420P, SWS_SPLINE, NULL, NULL, NULL); 
					sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);

					SDL_LockTexture(sdlTexture, NULL, (void **)&pixels1, &pitch1);
					memcpy(pixels1,             pFrameYUV->data[0], y_size  );
					memcpy(pixels1 + y_size,     pFrameYUV->data[2], y_size/4);
					memcpy(pixels1 + y_size*5/4, pFrameYUV->data[1], y_size/4);
					SDL_UnlockTexture(sdlTexture);

					SDL_UpdateTexture(sdlTexture, NULL, pixels1, pitch1);

					SDL_RenderClear(renderer);
					SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
					SDL_RenderPresent(renderer);
					/*SDL_LockYUVOverlay(bmp);
					bmp->pixels[0]=pFrameYUV->data[0];
					bmp->pixels[2]=pFrameYUV->data[1];
					bmp->pixels[1]=pFrameYUV->data[2];     
					bmp->pitches[0]=pFrameYUV->linesize[0];
					bmp->pitches[2]=pFrameYUV->linesize[1];   
					bmp->pitches[1]=pFrameYUV->linesize[2];
					SDL_UnlockYUVOverlay(bmp); 
					rect.x = 0;    
					rect.y = 0;    
					rect.w = pCodecCtx->width;    
					rect.h = pCodecCtx->height;    
					SDL_DisplayYUVOverlay(bmp, &rect); */
					//延时40ms
					SDL_Delay(40);
				}
			}
			av_free_packet(packet);
		}
		delete[] out_buffer;
		av_free(pFrameYUV);
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);

		return 0;
}
