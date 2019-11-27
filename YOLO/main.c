/**
*
*   File:   tutorial02.c
*           So our current plan is to replace the SaveFrame() function from
*           Tutorial 1, and instead output our frame to the screen. But first
*           we have to start by seeing how to use the SDL Library.
*           Uncommented lines of code refer to previous tutorials.
*
*           Compiled using
*               gcc -o tutorial02 tutorial02.c -lavutil -lavformat -lavcodec -lswscale -lz -lm  `sdl2-config --cflags --libs`
*           on Arch Linux.
*           sdl-config just prints out the proper flags for gcc to include the
*           SDL libraries properly. You may need to do something different to
*           get it to compile on your system; please check the SDL documentation
*           for your system. Once it compiles, go ahead and run it.
*
*           What happens when you run this program? The video is going crazy!
*           In fact, we're just displaying all the video frames as fast as we
*           can extract them from the movie file. We don't have any code right
*           now for figuring out when we need to display video. Eventually (in
*           Tutorial 5), we'll get around to syncing the video. But first we're
*           missing something even more important: sound!
*
*   Author: Rambod Rahmani <rambodrahmani@autistici.org>
*           Created on 8/10/18.
*
**/

#include <unistd.h>
#include <stdio.h>

//#include <libavcodec/avcodec.h>
//#include <libavutil/imgutils.h>
//#include <libavformat/avformat.h>
//#include <libswscale/swscale.h>
//#include <SDL2/SDL.h>
//#include <SDL2/SDL_thread.h>

#include "darknet.h"
#include <time.h>
#include <stdlib.h>


/**
 * Entry point.
 *
 * @param   argc    command line arguments counter.
 * @param   argv    command line arguments.
 *
 * @return          execution exit code.
 */

static void * cap;
static image buff [3];
static image buff_letter[3];
static int buff_index = 0;
static int demo_done = 0;
static int demo_total = 0;
double demo_time;

static float fps = 0;
static float demo_thresh = 0;
static float demo_hier = .5;
static network *net;
static int running = 0;
static float **predictions;
static int demo_index = 0;
static float *avg;
static int demo_frame = 3;
static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

int size_network(network *net)
{
    int i;
    int count = 0;
    for(i = 0; i < net->n; ++i){
        layer l = net->layers[i];
        if(l.type == YOLO || l.type == REGION || l.type == DETECTION){
            count += l.outputs;
        }
    }
    return count;
}
void remember_network(network *net)
{
    int i;
    int count = 0;
    for(i = 0; i < net->n; ++i){
        layer l = net->layers[i];
        if(l.type == YOLO || l.type == REGION || l.type == DETECTION){
            memcpy(predictions[demo_index] + count, net->layers[i].output, sizeof(float) * l.outputs);
            count += l.outputs;
        }
    }
}

void *fetch_in_thread(void *ptr)
{
    free_image(buff[buff_index]);
    buff[buff_index] = get_image_from_stream(cap);
    if(buff[buff_index].data == 0) {
        demo_done = 1;
        return 0;
    }
    letterbox_image_into(buff[buff_index], net->w, net->h, buff_letter[buff_index]);
    return 0;
}


void *display_in_thread(void *ptr)
{
    int c = show_image(buff[(buff_index + 1)%3], "Demo", 1);
    if (c != -1) c = c%256;
    if (c == 27) {
        demo_done = 1;
        return 0;
    } else if (c == 82) {
        demo_thresh += .02;
    } else if (c == 84) {
        demo_thresh -= .02;
        if(demo_thresh <= .02) demo_thresh = .02;
    } else if (c == 83) {
        demo_hier += .02;
    } else if (c == 81) {
        demo_hier -= .02;
        if(demo_hier <= .0) demo_hier = .0;
    }
    return 0;
}

detection *avg_predictions(network *net, int *nboxes)
{
    int i, j;
    int count = 0;
    fill_cpu(demo_total, 0, avg, 1);
    for(j = 0; j < demo_frame; ++j){
        axpy_cpu(demo_total, 1./demo_frame, predictions[j], 1, avg, 1);
    }
    for(i = 0; i < net->n; ++i){
        layer l = net->layers[i];
        if(l.type == YOLO || l.type == REGION || l.type == DETECTION){
            memcpy(l.output, avg + count, sizeof(float) * l.outputs);
            count += l.outputs;
        }
    }
    detection *dets = get_network_boxes(net, buff[0].w, buff[0].h, demo_thresh, demo_hier, 0, 1, nboxes);
    return dets;
}


void *detect_in_thread(void *ptr)
{
    running = 1;
    float nms = .4;

    layer l = net->layers[net->n-1];
    float *X = buff_letter[(buff_index+2)%3].data;
    network_predict(net, X);

    /*
       if(l.type == DETECTION){
       get_detection_boxes(l, 1, 1, demo_thresh, probs, boxes, 0);
       } else */
    remember_network(net);
    detection *dets = 0;
    int nboxes = 0;
    dets = avg_predictions(net, &nboxes);


    /*
       int i,j;
       box zero = {0};
       int classes = l.classes;
       for(i = 0; i < demo_detections; ++i){
       avg[i].objectness = 0;
       avg[i].bbox = zero;
       memset(avg[i].prob, 0, classes*sizeof(float));
       for(j = 0; j < demo_frame; ++j){
       axpy_cpu(classes, 1./demo_frame, dets[j][i].prob, 1, avg[i].prob, 1);
       avg[i].objectness += dets[j][i].objectness * 1./demo_frame;
       avg[i].bbox.x += dets[j][i].bbox.x * 1./demo_frame;
       avg[i].bbox.y += dets[j][i].bbox.y * 1./demo_frame;
       avg[i].bbox.w += dets[j][i].bbox.w * 1./demo_frame;
       avg[i].bbox.h += dets[j][i].bbox.h * 1./demo_frame;
       }
    //copy_cpu(classes, dets[0][i].prob, 1, avg[i].prob, 1);
    //avg[i].objectness = dets[0][i].objectness;
    }
     */

    if (nms > 0) do_nms_obj(dets, nboxes, l.classes, nms);

    printf("\033[2J");
    printf("\033[1;1H");
    printf("\nFPS:%.1f\n",fps);
    printf("Objects:\n\n");
    image display = buff[(buff_index+2) % 3];
    draw_detections(display, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes);
    free_detections(dets, nboxes);

    demo_index = (demo_index + 1)%demo_frame;
    running = 0;
    return 0;
}

int main(int argc, char * argv[])
{



    int count = 0;
//    void test_detector(char *datacfg, char *cfgfile, char *weightfile, char *filename, float thresh, float hier_thresh, char *outfile, int fullscreen)
//    float thresh = 0.5;
//    float hier_thresh = 0.5;
    int fullscreen = 0;
    list *options = read_data_cfg("config/coco.data");
    char *name_list = option_find_str(options, "names", "data/names.list");
    char **names = get_labels(name_list);

    image **alphabet = load_alphabet();
    net = load_network("config/yolov3.cfg", "config/yolov3.weights", 0);
    set_batch_network(net, 1);

    demo_names = names;
    demo_alphabet = alphabet;
    demo_thresh = 0.5;
    demo_hier = 0.5;
    srand(2222222);

    int i;
    demo_total = size_network(net);
    predictions = calloc(demo_frame, sizeof(float*));
    for (i = 0; i < demo_frame; ++i){
        predictions[i] = calloc(demo_total, sizeof(float));
    }
    avg = calloc(demo_total, sizeof(float));
    double time;
    char *input = "data/dog.jpg";
    float nms=.45;

    pthread_t detect_thread;
    pthread_t fetch_thread;

    layer l = net->layers[net->n-1];
    demo_classes = l.classes;

//    image im = load_image_color(input,0,0);
//    image sized = letterbox_image(im, net->w, net->h);

    cap = open_video_stream("Iron_Man-Trailer_HD.mp4", 0, 0, 0, 0);
    buff[0] = get_image_from_stream(cap);
    buff[1] = copy_image(buff[0]);
    buff[2] = copy_image(buff[0]);
    buff_letter[0] = letterbox_image(buff[0], net->w, net->h);
    buff_letter[1] = letterbox_image(buff[0], net->w, net->h);
    buff_letter[2] = letterbox_image(buff[0], net->w, net->h);

    make_window("Demo", buff[0].w, buff[0].h, fullscreen);




    while(!demo_done){
        buff_index = (buff_index + 1) %3;
        if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
        if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
        fps = 1./(what_time_is_it_now() - demo_time);
        demo_time = what_time_is_it_now();
        display_in_thread(0);
        pthread_join(fetch_thread, 0);
        pthread_join(detect_thread, 0);
        ++count;
    }
    //image sized = resize_image(im, net->w, net->h);
    //image sized2 = resize_max(im, net->w);
    //image sized = crop_image(sized2, -((net->w - sized2.w)/2), -((net->h - sized2.h)/2), net->w, net->h);
    //resize_network(net, sized.w, sized.h);



//    float *X = sized.data;
//    time=what_time_is_it_now();
//    network_predict(net, X);
//    printf("%s: Predicted in %f seconds.\n", input, what_time_is_it_now()-time);
//    int nboxes = 0;
//    detection *dets = get_network_boxes(net, im.w, im.h, thresh, hier_thresh, 0, 1, &nboxes);
//    //printf("%d\n", nboxes);
//    //if (nms) do_nms_obj(boxes, probs, l.w*l.h*l.n, l.classes, nms);
//    if (nms) do_nms_sort(dets, nboxes, l.classes, nms);
//    draw_detections(im, dets, nboxes, thresh, names, alphabet, l.classes);
//    free_detections(dets, nboxes);
//    make_window("predictions", 512, 512, 0);
//    show_image(im, "predictions", 0);
//    free_image(im);
//    free_image(sized);
    return 0;
}
