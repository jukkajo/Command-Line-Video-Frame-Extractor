#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>

/* Created: 02.11.2024
   Last modified: 11.11.2024
   @ Jukka J, jajoutzs@jyu.fi 
*/

// Function to save a single frame as a JPEG image
void save_frame_as_jpeg(AVCodecContext *jpegCtx, AVFrame *pFrame, int frame_number) {
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        return;
    }

    // Send frame to JPEG encoder
    if (avcodec_send_frame(jpegCtx, pFrame) == 0) {
        // Receive the encoded JPEG packet
        if (avcodec_receive_packet(jpegCtx, pkt) == 0) {
            char filename[64];
            snprintf(filename, sizeof(filename), "frame_%05d.jpg", frame_number);
            // Write JPEG packet to file
            FILE *jpegFile = fopen(filename, "wb");
            if (jpegFile) {
                fwrite(pkt->data, 1, pkt->size, jpegFile);
                fclose(jpegFile);
                printf("Saved %s\n", filename);
            } else {
                fprintf(stderr, "Could not open file %s for writing\n", filename);
            }
            av_packet_unref(pkt);
        } else {
            fprintf(stderr, "Failed to receive packet from JPEG encoder\n");
        }
    } else {
        fprintf(stderr, "Failed to send frame to JPEG encoder\n");
    }

    av_packet_free(&pkt);
}

int main(int argc, char *argv[]) {
    // Check for correct usage
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <input.mp4> <start_time> <end_time> <N>\n", argv[0]);
        return -1;
    }

    // Parse command-line arguments
    const char *input_file = argv[1];
    int start_time = atoi(argv[2]);
    int end_time = atoi(argv[3]);
    int N = atoi(argv[4]); // Save every Nth frame

    // Initialize FFmpeg libraries
    avformat_network_init();

    // Open input file and retrieve format context
    AVFormatContext *pFormatCtx = NULL;
    if (avformat_open_input(&pFormatCtx, input_file, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open file %s\n", input_file);
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }

    // Find the first video stream
    int video_stream_index = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        fprintf(stderr, "No video stream found\n");
        return -1;
    }

    // Get codec parameters and find decoder
    AVCodecParameters *pCodecParams = pFormatCtx->streams[video_stream_index]->codecpar;
    const AVCodec *pCodec = avcodec_find_decoder(pCodecParams->codec_id);
    if (!pCodec) {
        fprintf(stderr, "Unsupported codec\n");
        return -1;
    }

    // Allocate codec context for the decoder
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecCtx, pCodecParams) < 0) {
        fprintf(stderr, "Could not copy codec context\n");
        return -1;
    }

    // Open the decoder
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

    // Find the MJPEG encoder for saving frames as JPEG
    const AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) {
        fprintf(stderr, "JPEG codec not found\n");
        return -1;
    }

    // Allocate codec context for JPEG encoder
    AVCodecContext *jpegCtx = avcodec_alloc_context3(jpegCodec);
    if (!jpegCtx) {
        fprintf(stderr, "Could not allocate JPEG codec context\n");
        return -1;
    }

    // Set encoder parameters for JPEG format
    jpegCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    jpegCtx->height = pCodecCtx->height;
    jpegCtx->width = pCodecCtx->width;
    jpegCtx->time_base = (AVRational){1, 25};

    // Open JPEG encoder
    if (avcodec_open2(jpegCtx, jpegCodec, NULL) < 0) {
        fprintf(stderr, "Could not open JPEG codec\n");
        avcodec_free_context(&jpegCtx);
        return -1;
    }

    // Allocate packet and frame
    AVPacket *packet = av_packet_alloc();
    AVFrame *pFrame = av_frame_alloc();

    int frame_count = 0;           // Counter for processed frames
    int saved_frame_count = 0;      // Counter for saved frames
    int64_t start_pts = start_time * AV_TIME_BASE; // Convert start time to PTS

    // Seek to start time
    av_seek_frame(pFormatCtx, video_stream_index, start_pts, AVSEEK_FLAG_BACKWARD);

    int reached_end = 0; // Flag to stop processing at end time
    while (av_read_frame(pFormatCtx, packet) >= 0 && !reached_end) {
        if (packet->stream_index == video_stream_index) {
            if (avcodec_send_packet(pCodecCtx, packet) >= 0) {
                while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                    // Calculate frame time in seconds
                    double frame_time = pFrame->pts * av_q2d(pFormatCtx->streams[video_stream_index]->time_base);

                    // Check if frame is within start and end time range
                    if (frame_time >= start_time && frame_time <= end_time) {
                        // Save every Nth frame
                        if (frame_count % N == 0) {
                            save_frame_as_jpeg(jpegCtx, pFrame, saved_frame_count++);
                        }
                        frame_count++; // Increment processed frame count
                    }
                    // Stop if frame exceeds end time
                    if (frame_time > end_time) {
                        reached_end = 1;
                        break;
                    }
                    av_frame_unref(pFrame);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Clean up and free allocated resources
    avcodec_free_context(&jpegCtx);
    av_frame_free(&pFrame);
    av_packet_free(&packet);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_network_deinit();

    printf("Frames saved as JPEGs from %d to %d seconds.\n", start_time, end_time);

    return 0;
}

