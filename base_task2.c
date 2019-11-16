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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


void printHelpMenu();
/* Next, we define period and execution cost to be constant. 
 * These are only constants for convenience in this example, they can be
 * determined at run time, e.g., from command line parameters.
 *
 * These are in milliseconds.
 */
// #define PERIOD            50
// #define RELATIVE_DEADLINE 50
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

AVFormatContext * pFormatCtx;
AVCodecContext * pCodecCtx;
int videoStream;
AVCodec * pCodec;
AVFrame * pFrame;
AVFrame * pict;
SDL_Renderer * renderer;
SDL_Texture * texture;
AVPacket * pPacket;
struct SwsContext * sws_ctx;
SDL_Event event;
// int maxFramesToDecode;


int main(int argc, char** argv)
{
	int ret;
	int i;
	int do_exit;
	struct rt_task param;
	int numBytes;
    uint8_t * buffer = NULL;
    SDL_Window * screen;
    
    double fps;
    double period;

	if ( !(argc > 2) ){
        printHelpMenu();
        return -1;
    }
    

	/* Setup task parameters */
	init_rt_task_param(&param);
	param.exec_cost = ms2ns(EXEC_COST);
	

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
	pFormatCtx = NULL;
	pCodecCtx = NULL;
	pCodec = NULL;
	pFrame = NULL;
	/*****
	 * 2) Work environment (e.g., global data structures, file data, etc.) would
	 *    be setup here.
	 */
	ret = -1;
	ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);   // [1]
    if (ret != 0){
        // error while initializing SDL
        printf("Could not initialize SDL - %s\n.", SDL_GetError());

        // exit with error
        return -1;
    }

    av_register_all();
    
    ret = avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
    if (ret < 0){
       	printf("couldnot open file!");
        return -1;
    }

    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (ret < 0){
        printf("Could not find stream information %s\n", argv[1]);
        return -1;
    }

    av_dump_format(pFormatCtx, 0, argv[1], 0);

    
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++){
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1){
        printf("Could not find video stream.");
        return -1;
    }

    
    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == NULL){
        printf("Unsupported codec!\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    ret = avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);
    if (ret != 0){
        printf("Could not copy codec context.\n");
        return -1;
    }

    ret = avcodec_open2(pCodecCtx, pCodec, NULL);
    if (ret < 0){
        printf("Could not open codec.\n");
        return -1;
    }

    
    pFrame = av_frame_alloc();
    if (pFrame == NULL){
        printf("Could not allocate frame.\n");
        return -1;
    }
    fps = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);

                    // get clip sleep time
    period = 1000.0/(double)fps;
    param.period = ms2ns(period);
	param.relative_deadline = ms2ns(period);

    // Create a window with the specified position, dimensions, and flags.
    screen = SDL_CreateWindow( // [2]
                            "SDL Video Player",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            pCodecCtx->width/2,
                            pCodecCtx->height/2,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!screen){
        // could not set video mode
        printf("SDL: could not set video mode - exiting.\n");

        // exit with Error
        return -1;
    }

    //
    SDL_GL_SetSwapInterval(1);

    // A structure that contains a rendering state.
    renderer = NULL;

    // Use this function to create a 2D rendering context for a window.
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);   // [3]

    // A structure that contains an efficient, driver-specific representation
    // of pixel data.
    texture = NULL;

    // Use this function to create a texture for a rendering context.
    texture = SDL_CreateTexture(  // [4]
                renderer,
                SDL_PIXELFORMAT_YV12,
                SDL_TEXTUREACCESS_STREAMING,
                pCodecCtx->width,
                pCodecCtx->height
            );

    sws_ctx = NULL;
    pPacket = av_packet_alloc();
    if (pPacket == NULL){
        printf("Could not alloc packet,\n");
        return -1;
    }

    // set up our SWSContext to convert the image data to YUV420:
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

    /**
     * As we said before, we are using YV12 to display the image, and getting
     * YUV420 data from ffmpeg.
     */

    

    numBytes = av_image_get_buffer_size(
                AV_PIX_FMT_YUV420P,
                pCodecCtx->width,
                pCodecCtx->height,
                32
            );
    buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    // used later to handle quit event
    

    pict = av_frame_alloc();

    av_image_fill_arrays(
        pict->data,
        pict->linesize,
        buffer,
        AV_PIX_FMT_YUV420P,
        pCodecCtx->width,
        pCodecCtx->height,
        32
    );

    
    // sscanf(argv[2], "%d", &maxFramesToDecode);


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
	CALL( task_mode(LITMUS_RT_TASK) );

	/* The task is now executing as a real-time task if the call didn't fail. 
	 */
	
	CALL( wait_for_ts_release());

	/*****
	 * 5) Invoke real-time jobs.
	 */
	do {
		/* Wait until the next job is released. */
		sleep_next_period();
		/* Invoke job. */
		do_exit = job();
		

		
		
	} while (!do_exit);


	
	/*****
	 * 6) Transition to background mode.
	 */
	CALL( task_mode(BACKGROUND_TASK) );



	/***** 
	 * 7) Clean up, maybe print results and stats, and exit.
	 */

	av_frame_free(&pFrame);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    SDL_DestroyRenderer(renderer);
    SDL_Quit();
	return 0;
}

void printHelpMenu(){
    printf("Invalid arguments.\n\n");
    printf("Usage: ./tutorial02 <filename> <max-frames-to-decode>\n\n");
    printf("e.g: ./tutorial02 /home/rambodrahmani/Videos/video.mp4 200\n");
}

int job(void){
	/* Do real-time calculation. */
	int i = 0;
	int ret = -1;
	SDL_Rect rect;
    while(av_read_frame(pFormatCtx, pPacket) >= 0){
        if (pPacket->stream_index == videoStream){
            ret = avcodec_send_packet(pCodecCtx, pPacket);
            if (ret < 0){
                printf("Error sending packet for decoding.\n");
                return 1;
            }
            else{
                ret = avcodec_receive_frame(pCodecCtx, pFrame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    continue;
                else if (ret < 0){
                    printf("Error while decoding.\n");
                    return 1;
                }

                // Convert the image into YUV format that SDL uses:
                // We change the conversion format to PIX_FMT_YUV420P, and we
                // use sws_scale just like before.
                sws_scale(
                    sws_ctx,
                    (uint8_t const * const *)pFrame->data,
                    pFrame->linesize,
                    0,
                    pCodecCtx->height,
                    pict->data,
                    pict->linesize
                );
                // get clip fps
                // sleep: usleep won't work when using SDL_CreateWindow
                // usleep(sleep_time);
                // Use SDL_Delay in milliseconds to allow for cpu scheduling
                // SDL_Delay((1000 * sleep_time) - 10);    // [5]

                // The simplest struct in SDL. It contains only four shorts. x, y which
                // holds the position and w, h which holds width and height.It's important
                // to note that 0, 0 is the upper-left corner in SDL. So a higher y-value
                // means lower, and the bottom-right corner will have the coordinate x + w,
                // y + h.
                
                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;

                // printf(
                //     "Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
                //     av_get_picture_type_char(pFrame->pict_type),
                //     pCodecCtx->frame_number,
                //     pFrame->pts,
                //     pFrame->pkt_dts,
                //     pFrame->key_frame,
                //     pFrame->coded_picture_number,
                //     pFrame->display_picture_number,
                //     pCodecCtx->width,
                //     pCodecCtx->height
                // );

                // Use this function to update a rectangle within a planar
                // YV12 or IYUV texture with new pixel data.
                SDL_UpdateYUVTexture(
                    texture,            // the texture to update
                    &rect,              // a pointer to the rectangle of pixels to update, or NULL to update the entire texture
                    pict->data[0],      // the raw pixel data for the Y plane
                    pict->linesize[0],  // the number of bytes between rows of pixel data for the Y plane
                    pict->data[1],      // the raw pixel data for the U plane
                    pict->linesize[1],  // the number of bytes between rows of pixel data for the U plane
                    pict->data[2],      // the raw pixel data for the V plane
                    pict->linesize[2]   // the number of bytes between rows of pixel data for the V plane
                );

                // clear the current rendering target with the drawing color
                SDL_RenderClear(renderer);

                // copy a portion of the texture to the current rendering target
                SDL_RenderCopy(
                    renderer,   // the rendering context
                    texture,    // the source texture
                    NULL,       // the source SDL_Rect structure or NULL for the entire texture
                    NULL        // the destination SDL_Rect structure or NULL for the entire rendering
                                // target; the texture will be stretched to fill the given rectangle
                );

                // update the screen with any rendering performed since the previous call
                SDL_RenderPresent(renderer);
                i++;
                break;
            }
        }
        av_packet_unref(pPacket);

        // handle Ctrl + C event
        SDL_PollEvent(&event);
        switch(event.type){
            case SDL_QUIT:{
                SDL_Quit();
                return 1;
            }
            break;

            default:{
                // nothing to do
            }
            break;
        }
    }

    return 1-i;
}
