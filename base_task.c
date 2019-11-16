/* based_task.c -- A basic real-time task skeleton. 
 *
 * This (by itself useless) task demos how to setup a 
 * single-threaded LITMUS^RT real-time task.
 */

/* First, we include standard headers.
 * Generally speaking, a LITMUS^RT real-time task can perform any
 * system call, etc., but no real-time guarantees can be made if a
 * system call blocks. To be on the safe side, only use I/O for debugging
 * purposes and from non-real-time sections.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

/* Second, we include the LITMUS^RT user space library header.
 * This header, part of liblitmus, provides the user space API of
 * LITMUS^RT.
 */
#include "litmus.h"

/* Next, we define period and execution cost to be constant. 
 * These are only constants for convenience in this example, they can be
 * determined at run time, e.g., from command line parameters.
 *
 * These are in milliseconds.
 */
#define PERIOD            100
#define RELATIVE_DEADLINE 100
#define EXEC_COST         10

/* Catch errors.
 */
#define CALL( exp ) do { \
		int ret; \
		ret = exp; \
		if (ret != 0) \
			fprintf(stderr, "%s failed: %m\n", #exp);\
		else \
			fprintf(stderr, "%s ok.\n", #exp); \
	} while (0)


/* Declare the periodically invoked job. 
 * Returns 1 -> task should exit.
 *         0 -> task should continue.
 */
int job(void);

/* typically, main() does a couple of things: 
 * 	1) parse command line parameters, etc.
 *	2) Setup work environment.
 *	3) Setup real-time parameters.
 *	4) Transition to real-time mode.
 *	5) Invoke periodic or sporadic jobs.
 *	6) Transition to background mode.
 *	7) Clean up and exit.
 *
 * The following main() function provides the basic skeleton of a single-threaded
 * LITMUS^RT real-time task. In a real program, all the return values should be 
 * checked for errors.
 */
void printHelpMenu()
{
    printf("Invalid arguments.\n\n");
    printf("Usage: ./tutorial02 <filename> <max-frames-to-decode>\n\n");
    printf("e.g: ./tutorial02 /home/abhisek/Videos/video.mp4 200\n");
}
int main(int argc, char** argv)
{
	//int do_exit;
	struct rt_task param;
	int ret = -1;
	AVFormatContext * pFormatCtx = NULL;
    	int i;
    	int numBytes;
    	uint8_t * buffer = NULL;
    	AVCodecContext * pCodecCtx = NULL;
    	AVFrame * pict = av_frame_alloc();
	AVFrame * pFrame = NULL;
    	int videoStream = -1;
	int maxFramesToDecode;
	AVCodec * pCodec = NULL;
	SDL_Renderer * renderer = NULL;
	SDL_Texture * texture = NULL;
	SDL_Event event;
	SDL_Rect rect;
    	SDL_Window * screen = NULL; 
	struct SwsContext * sws_ctx = NULL;
    	AVPacket * pPacket = av_packet_alloc();

	/* Setup task parameters */
	init_rt_task_param(&param);
	param.exec_cost = ms2ns(EXEC_COST);
	param.period = ms2ns(PERIOD);
	param.relative_deadline = ms2ns(RELATIVE_DEADLINE);

	/* What to do in the case of budget overruns? */
	param.budget_policy = NO_ENFORCEMENT;

	/* The task class parameter is ignored by most plugins. */
	param.cls = RT_CLASS_SOFT;

	/* The priority parameter is only used by fixed-priority plugins. */
	param.priority = LITMUS_LOWEST_PRIORITY;

	/* The task is in background mode upon startup. */


	/*****
	 * 1) Command line paramter parsing would be done here.
	 */



	/*****
	 * 2) Work environment (e.g., global data structures, file data, etc.) would
	 *    be setup here.
	 */



	/*****
	 * 3) Setup real-time parameters. 
	 *    In this example, we create a sporadic task that does not specify a 
	 *    target partition (and thus is intended to run under global scheduling). 
	 *    If this were to execute under a partitioned scheduler, it would be assigned
	 *    to the first partition (since partitioning is performed offline).
	 */
	CALL( init_litmus() );

	/* To specify a partition, do
	 *
	 * param.cpu = CPU;
	 * be_migrate_to(CPU);
	 *
	 * where CPU ranges from 0 to "Number of CPUs" - 1 before calling
	 * set_rt_task_param().
	 */
	CALL( set_rt_task_param(gettid(), &param) );


	/*****
	 * 4) Transition to real-time mode.
	 */

	/* The task is now executing as a real-time task if the call didn't fail. 
	 */



    

    ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);   // [1]
    if (ret != 0)
    {
        printf("Could not initialize SDL - %s\n.", SDL_GetError());

        return -1;
    }

    
    ret = avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
    if (ret < 0)
    {
        printf("Could not open file %s\n", argv[1]);
        return -1;
    }

    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (ret < 0)
    {
        printf("Could not find stream information %s\n", argv[1]);
        return -1;
    }

    av_dump_format(pFormatCtx, 0, argv[1], 0);


    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1)
    {
        printf("Could not find video stream.");
        return -1;
    }

    
    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == NULL)
    {
        printf("Unsupported codec!\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    ret = avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);
    if (ret != 0)
    {
        printf("Could not copy codec context.\n");
        return -1;
    }

    ret = avcodec_open2(pCodecCtx, pCodec, NULL);
    if (ret < 0)
    {
        printf("Could not open codec.\n");
        return -1;
    }

    
    pFrame = av_frame_alloc();
    if (pFrame == NULL)
    {
        printf("Could not allocate frame.\n");
        return -1;
    }

    screen = SDL_CreateWindow(
                            "SDL Video Player",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            pCodecCtx->width/2,
                            pCodecCtx->height/2,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
        );

    if (!screen)
    {
        printf("SDL: could not set video mode - exiting.\n");

        return -1;
    }

    SDL_GL_SetSwapInterval(1);

    

    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE); 

    

    texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_YV12,
                SDL_TEXTUREACCESS_STREAMING,
                pCodecCtx->width,
                pCodecCtx->height
            );


    if (pPacket == NULL)
    {
        printf("Could not alloc packet,\n");
        return -1;
    }

    sws_ctx = sws_getContext(
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );




    numBytes = av_image_get_buffer_size(
                AV_PIX_FMT_YUV420P,
                pCodecCtx->width,
                pCodecCtx->height,
                32
            );
    buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    



    av_image_fill_arrays(
        pict->data,
        pict->linesize,
        buffer,
        AV_PIX_FMT_YUV420P,
        pCodecCtx->width,
        pCodecCtx->height,
        32
    );

    
    sscanf(argv[2], "%d", &maxFramesToDecode);

    i = 0;
	CALL( task_mode(LITMUS_RT_TASK) );
	CALL(wait_for_ts_release());

    while (av_read_frame(pFormatCtx, pPacket) >= 0)
    {
	
	
        if (pPacket->stream_index == videoStream)
        {
            ret = avcodec_send_packet(pCodecCtx, pPacket);
            if (ret < 0)
            {
                printf("Error sending packet for decoding.\n");
                return -1;
            }

            while (ret >= 0)
            {
		sleep_next_period();

                ret = avcodec_receive_frame(pCodecCtx, pFrame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    printf("Error while decoding.\n");
                    return -1;
                }

                sws_scale(
                    sws_ctx,
                    (uint8_t const * const *)pFrame->data,
                    pFrame->linesize,
                    0,
                    pCodecCtx->height,
                    pict->data,
                    pict->linesize
                );

                if (++i <= maxFramesToDecode)
                {
                    double fps = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);

                    double sleep_time = 1.0/(double)fps;

                    SDL_Delay((1000 * sleep_time) - 10);  

                    rect.x = 0;
                    rect.y = 0;
                    rect.w = pCodecCtx->width;
                    rect.h = pCodecCtx->height;

                    printf(
                        "Frame %c (%d) pts %ld dts %ld key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
                        av_get_picture_type_char(pFrame->pict_type),
                        pCodecCtx->frame_number,
                        pFrame->pts,
                        pFrame->pkt_dts,
                        pFrame->key_frame,
                        pFrame->coded_picture_number,
                        pFrame->display_picture_number,
                        pCodecCtx->width,
                        pCodecCtx->height
                    );

                    SDL_UpdateYUVTexture(
                        texture,            
                        &rect,              
                        pict->data[0],      
                        pict->linesize[0],  
                        pict->data[1],      
                        pict->linesize[1],  
                        pict->data[2],      
                        pict->linesize[2] 
                    );
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(
                        renderer,   
                        texture,    
                        NULL,       
                        NULL        
                    );

                    SDL_RenderPresent(renderer);
                }
                else
                {
                   //break;
                }
            }

            if (i > maxFramesToDecode)
            {
                //break;
            }
        }
        av_packet_unref(pPacket);

        SDL_PollEvent(&event);
        switch(event.type)
        {
            case SDL_QUIT:
            {
                SDL_Quit();
                exit(0);
            }
            break;

            default:
            {
            }
            break;
        }
    }




	
	/*****
	 * 6) Transition to background mode.
	 */
	CALL( task_mode(BACKGROUND_TASK) );

    	av_frame_free(&pFrame);
    	av_free(pFrame);
    	avcodec_close(pCodecCtx);
    	avformat_close_input(&pFormatCtx);

    	SDL_DestroyRenderer(renderer);
    	SDL_Quit();

	/***** 
	 * 7) Clean up, maybe print results and stats, and exit.
	 */
	return 0;
}


int job(void) 
{
	/* Do real-time calculation. */

	/* Don't exit. */
	return 0;
}
