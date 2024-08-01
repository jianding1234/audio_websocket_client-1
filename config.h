#ifndef CONFIG_H
#define CONFIG_H
#include <vector>

#define USE_DEVICE_NUM  1  // 1-default 2....others
#define WEBSOCKET_HOST "3.106.211.104"
#define WEBSOCKET_PORT "8765"
#define WEBSOCKET_HOST_PORT "127.0.0.1:8765"
//#define SAMPLE_RATE 44100
#define SAMPLE_RATE 20000
#define FRAMES_PER_BUFFER 256
#define NUM_CHANNELS 1
typedef std::vector<float> AudioBuffer;
extern void audioProcess(AudioBuffer inputBuffer);
#endif // CONFIG_H
