#include <fstream>
#include <iostream>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define DEBUG 1
#define WAVE_HEADER_LEN 44
// A struct to hold the RIFF data of the WAV file
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

struct AudioConverter{
    std::string m_file;
    std::string m_ip;
    uint16_t m_port;
    uint16_t m_delay;
    struct WaveFile* m_wav;
};

struct AudioConverter* init(int argc, char* argv[])
{
    // arguments would be like this:
    // audio_file_path ip port delay
    if(argc != 5) return NULL;
    struct AudioConverter* ctx = (struct AudioConverter*) malloc(sizeof(struct AudioConverter));

    ctx->m_file = argv[1];
    ctx->m_ip = argv[2];
    ctx->m_port = std::atoi( argv[3] );
    ctx->m_delay = std::atoi( argv[4] );

    ctx->m_wav = (struct WaveFile*) malloc(sizeof(struct WaveFile));

#if defined(DEBUG)
    std::cout << "File path: " << ctx->m_file << std::endl;
    std::cout << "IP: " << ctx->m_ip << std::endl;
    std::cout << "Port: " << ctx->m_port << std::endl;
    std::cout << "Delay: " << ctx->m_delay << std::endl;
#endif
    return ctx;
}

int parse_audio(struct AudioConverter* ctx)
{

    std::ifstream file(ctx->m_file, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        std::cout << "Could not open file!\n";
        return EXIT_FAILURE;
    }

    file.read(reinterpret_cast<char*>(ctx->m_wav), WAVE_HEADER_LEN);

#if defined(DEBUG)
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
    file.read(ctx->m_wav->audio_bytes, ctx->m_wav->audio_len);

    file.close();

    return EXIT_SUCCESS; 
}

int broadcast_on_udp(struct AudioConverter* ctx)
{

    int sockfd;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ctx->m_port); // port number

    if(inet_pton(AF_INET, ctx->m_ip.c_str() , &server_addr.sin_addr.s_addr) <= 0)
    {
        std::cerr << "Invalid IP address" << std::endl;
        return EXIT_FAILURE;
    }

    const size_t max_data_length = 480; // For example
    for (size_t i = 0; i < ctx->m_wav->audio_len; i += max_data_length) {
        size_t length = std::min(max_data_length, ctx->m_wav->audio_len - i);
        sendto(sockfd, &ctx->m_wav->audio_bytes[i], length, MSG_CONFIRM,
               (const struct sockaddr *) &server_addr, sizeof(server_addr));
        if(ctx->m_delay > 0)
            usleep(ctx->m_delay*1000);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}

int free_converter(struct AudioConverter* ctx)
{
    if(ctx != NULL)
    {
        if(ctx->m_wav != NULL)
        {
            if(ctx->m_wav->audio_bytes != NULL)
            {
                free(ctx->m_wav->audio_bytes);
            }
            free(ctx->m_wav);
        }
        free(ctx);
    }
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {

    auto converter = init(argc, argv);
    if(converter == NULL)
    {
        std::cerr << "Invalid argument number" << std::endl;
        free_converter(converter);
        return EXIT_FAILURE;
    }

    auto parsed_audio = parse_audio(converter);
    if(parsed_audio != 0)
    {
        std::cerr << "Invalid file" << std::endl;
        free_converter(converter);
        return EXIT_FAILURE;
    }

    if(broadcast_on_udp(converter) != EXIT_SUCCESS)
    {
        std::cerr << "Socket creation error" << std::endl;
        free_converter(converter);
        return EXIT_FAILURE;
    }
    
    free_converter(converter);
    return EXIT_SUCCESS;
}
