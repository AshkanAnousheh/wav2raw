#include <fstream>
#include <iostream>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#include "filter.h"

#define DEBUG 1
#define USE_RTP_HEADER 0
#define AUDIO_BIG_ENDIAN 0
#define WAVE_HEADER_LEN 44

#define BASH_RED_COLOR "\033[31m"
#define BASH_GREEN_COLOR "\033[32m"
#define BASH_YELLOW_COLOR "\033[33m"
#define BASH_RESET_COLOR "\033[0m"
                                //  2,3             4,5,6,7           8,9,10,11
// A struct to hold the RIFF data of the WAV file
struct AudioContext;

int broadcast_on_udp(struct AudioContext* ctx);
int create_output_file(struct AudioContext* ctx);


typedef int (*OutputHandler)(struct AudioContext* ctx);
typedef int (*ProcessHandler)(struct AudioContext* ctx);

struct WaveFile {
    char riff_header[4]; // Contains "RIFF"
    int wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"

    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    int fmt_chunk_size; // Should be 16 for PCM
    short audio_format; // Should be 1 for PCM. 3 for IEEE Float
    short num_channels;
    int sample_rate;
    int byte_rate;  // Number of bytes per second. sample_rate * num_channels * BytesPerSample
    short sample_alignment; // num_channels * BytesPerSample
    short bit_depth; // Number of bits per sample

    char data_header[4]; // Contains "data"
    int audio_len; // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wave file is bytes
    char* audio_bytes;
};

struct StreamConfig
{
    std::string m_in_file, m_out_file;
    std::string m_ip;
    uint16_t m_port;
    uint32_t m_delay;
};

struct AudioContext{
    OutputHandler handle_output;
    ProcessHandler handle_filter;
    struct WaveFile* m_wav;
    struct StreamConfig* m_conf;
};

int parse_audio(struct AudioContext* ctx)
{

    std::ifstream file(ctx->m_conf->m_in_file, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        std::cout << "Could not open file!\n";
        return EXIT_FAILURE;
    }

    file.read(reinterpret_cast<char*>(ctx->m_wav), WAVE_HEADER_LEN);

#if (DEBUG)
    std::cout << "** Header information of the file **" << std::endl;
    std::cout << "Header name: " << std::string(ctx->m_wav->riff_header,4) << std::endl;
    std::cout << "Wave name: " << std::string(ctx->m_wav->wave_header,4) << std::endl;
    std::cout << "Number of channels: " << std::to_string(ctx->m_wav->num_channels) << std::endl;
    std::cout << "Sample rate: " << std::to_string(ctx->m_wav->sample_rate) << std::endl;
    std::cout << "Number of bits per sample: " << std::to_string(ctx->m_wav->bit_depth) << std::endl;
    std::cout << "Number of data byte(s): " << std::to_string(ctx->m_wav->audio_len) << std::endl;
    std::cout << "** End of Header information **" << std::endl;
#endif

    ctx->m_wav->audio_bytes = (char*) malloc(ctx->m_wav->audio_len);
    if(ctx->m_wav->sample_rate != 48000)
        std::cout << BASH_RED_COLOR << "Your sample rate is wrong!" << BASH_RESET_COLOR << std::endl;

    if(ctx->m_wav->bit_depth != 16)
        std::cout << BASH_RED_COLOR << "Your bit depth is wrong!" << BASH_RESET_COLOR << std::endl;

    if(ctx->m_wav->audio_len % 2 != 0)
        std::cout << BASH_RED_COLOR << "Your sound len is wrong!" << BASH_RESET_COLOR << std::endl;

    file.read(ctx->m_wav->audio_bytes, ctx->m_wav->audio_len);

#if (AUDIO_BIG_ENDIAN)
    for(size_t i = 0; i < ctx->m_wav->audio_len ; i+=2)
        std::swap(ctx->m_wav->audio_bytes[i], ctx->m_wav->audio_bytes[i+1]);
#endif 

    file.close();

    return EXIT_SUCCESS; 
}

int apply_filter(struct AudioContext* ctx)
{
    std::cout << BASH_GREEN_COLOR << "Apply filtering..." << BASH_RESET_COLOR << std::endl;
    int16_t* temp_buff = (int16_t*) malloc(ctx->m_wav->audio_len);

    for (size_t i = 0; i < ctx->m_wav->audio_len; i+=2)
    {
        temp_buff[i/2] = (ctx->m_wav->audio_bytes[i+1]<<8) | ctx->m_wav->audio_bytes[i]; 
    }

    for (size_t i = 0; i < ctx->m_wav->audio_len/2; i++)
    {
        temp_buff[i] = filter(temp_buff[i]);
    }

    for (size_t i = 0; i < ctx->m_wav->audio_len; i+=2)
    {
        ctx->m_wav->audio_bytes[i+1] = (temp_buff[i/2] & 0xFF00) >> 8;
        ctx->m_wav->audio_bytes[i] = temp_buff[i/2] & 0xFF;
    }

    free(temp_buff);
    return EXIT_SUCCESS;
}

int create_output_file(struct AudioContext* ctx)
{
    std::cout << BASH_GREEN_COLOR << "Creating output file..." << BASH_RESET_COLOR << std::endl;

    std::ofstream out_file(ctx->m_conf->m_out_file,  std::ios::binary | std::ios::out);
    // size_t data_len = ctx->m_wav->audio_len;
    out_file.write(reinterpret_cast<char*>(ctx->m_wav), WAVE_HEADER_LEN);
    out_file.write(ctx->m_wav->audio_bytes, ctx->m_wav->audio_len);
    // std::cout << BASH_RED_COLOR << "Debug 2 !" << BASH_RESET_COLOR << std::endl;
    out_file.close();

    return EXIT_SUCCESS;
}

int broadcast_on_udp(struct AudioContext* ctx)
{

    int sockfd;
    struct sockaddr_in server_addr;
    std::cout << BASH_GREEN_COLOR << "Broadcasting on UDP..." << BASH_RESET_COLOR << std::endl;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ctx->m_conf->m_port); // port number

    if(inet_pton(AF_INET, ctx->m_conf->m_ip.c_str() , &server_addr.sin_addr.s_addr) <= 0)
    {
        std::cerr << "Invalid IP address" << std::endl;
        return EXIT_FAILURE;
    }


    uint16_t sequence_number = 0;
    uint32_t timestamp = 0;
    uint32_t rand_number = 0;

    char sof[12] = {0};


    const size_t max_data_length = 480; // For example
    char* buffer = (char*) malloc(max_data_length + sizeof(sof));
    
    for (size_t i = 0; i < ctx->m_wav->audio_len; i += max_data_length) {

        size_t length = std::min(max_data_length, ctx->m_wav->audio_len - i);
        
#if (USE_RTP_HEADER)
        
        sof[0] = 0x80;
        sof[1] = 0x60;
        sof[2] = (sequence_number&0xFF00)>>8;
        sof[3] = (sequence_number&0xFF);
        sof[4] = (timestamp&0xFF000000)>>24;
        sof[5] = (timestamp&0xFF0000)>>16;
        sof[6] = (timestamp&0xFF00)>>8;
        sof[7] = (timestamp&0xFF);
        sof[8] = 0;
        sof[9] = 0;
        sof[10] = 0;
        sof[11] = 96;
        
        length += 12;
        
        memcpy(buffer, sof, 12);
        memcpy(&buffer[12], &ctx->m_wav->audio_bytes[i], max_data_length);
#else
        memcpy(buffer, &ctx->m_wav->audio_bytes[i], max_data_length);
#endif
        sendto(sockfd, buffer, length, MSG_CONFIRM,
               (const struct sockaddr *) &server_addr, sizeof(server_addr));
        sequence_number++;
        timestamp += 120;
        if(ctx->m_conf->m_delay > 0)
            usleep(ctx->m_conf->m_delay);
    }

    free(buffer);
    close(sockfd);
    return EXIT_SUCCESS;
}


int main(int argc, char* argv[]) {
    
    struct AudioContext audio_context;
    struct StreamConfig config;
    struct WaveFile wave_file;

    if(argc < 3)
    {
        std::cerr << "Arg number error" << std::endl;
        return EXIT_FAILURE;
    }

    audio_context.m_conf = &config;
    audio_context.handle_filter = apply_filter;
    audio_context.m_wav = &wave_file;


    std::string output_type = argv[1];
    
    if(output_type == "udp")
    {
        audio_context.handle_output = broadcast_on_udp;
        audio_context.m_conf->m_in_file = argv[2];
        audio_context.m_conf->m_ip = argv[3];
        audio_context.m_conf->m_port = std::atoi( argv[4] );
        audio_context.m_conf->m_delay = std::atoi( argv[5] );
    }
    else if( output_type == "file")
    {
        audio_context.handle_output = create_output_file;
        audio_context.m_conf->m_in_file = argv[2];
        audio_context.m_conf->m_out_file = argv[3];
    }

    int res = parse_audio( &audio_context );

    res = audio_context.handle_filter(&audio_context);

    res = audio_context.handle_output(&audio_context);

    return EXIT_SUCCESS;
}
