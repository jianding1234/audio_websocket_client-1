#include <iostream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <portaudio.h>
#include "config.h"

class AudioBuffer {
public:
    void insert(const float* data, size_t size) {
        buffer.insert(buffer.end(), data, data + size);
    }

    bool empty() const {
        return buffer.empty();
    }

    void clear() {
        buffer.clear();
    }

    const std::vector<float>& get() const {
        return buffer;
    }

private:
    std::vector<float> buffer;
};

class AudioStream {
public:
    AudioStream(AudioBuffer* audioBuffer) : audioBuffer(audioBuffer), stream(nullptr) {
        Pa_Initialize();
        configureStream();
    }

    ~AudioStream() {
        stopStream();
        Pa_Terminate();
    }

    void startStream() {
        PaError err = Pa_StartStream(stream);
        if (err != paNoError) {
            throw std::runtime_error("Failed to start audio stream: " + std::string(Pa_GetErrorText(err)));
        }
    }

    void stopStream() {
        if (stream) {
            PaError err = Pa_StopStream(stream);
            if (err != paNoError) {
                std::cerr << "Failed to stop audio stream: " << Pa_GetErrorText(err) << std::endl;
            }
            err = Pa_CloseStream(stream);
            if (err != paNoError) {
                std::cerr << "Failed to close audio stream: " << Pa_GetErrorText(err) << std::endl;
            }
        }
    }

private:
    static int audioCallback(const void* inputBuffer, void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData) {
        AudioBuffer* audioBuffer = static_cast<AudioBuffer*>(userData);
        const float* input = static_cast<const float*>(inputBuffer);
        audioBuffer->insert(input, framesPerBuffer);
        return paContinue;
    }

    void configureStream() {
        PaStreamParameters inputParameters;
        inputParameters.device = USE_DEVICE_NUM < Pa_GetDeviceCount() ? USE_DEVICE_NUM : Pa_GetDefaultInputDevice();
        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        PaError err = Pa_OpenStream(&stream, &inputParameters, nullptr, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, audioCallback, audioBuffer);
        if (err != paNoError) {
            throw std::runtime_error("Failed to open audio stream: " + std::string(Pa_GetErrorText(err)));
        }
    }

    PaStream* stream;
    AudioBuffer* audioBuffer;
};

class WebSocketClient {
public:
    WebSocketClient() : ws(ioc) {
        boost::asio::ip::tcp::resolver resolver(ioc);
        auto endpoints = resolver.resolve(WEBSOCKET_HOST, WEBSOCKET_PORT);
        boost::asio::connect(ws.next_layer(), endpoints);
        ws.handshake(WEBSOCKET_HOST_PORT, "/");
    }

    ~WebSocketClient() {
        ws.close(boost::beast::websocket::close_code::normal);
    }

    void sendAudioData(const std::vector<float>& data) {
        ws.write(boost::asio::buffer(data));
        boost::beast::flat_buffer buffer;
        ws.read(buffer);
        std::string response = boost::beast::buffers_to_string(buffer.data());
        logFile << response << std::endl;
    }

private:
    boost::asio::io_context ioc;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws;
    std::ofstream logFile{ "host.log" };
};

class AudioProcessor {
public:
    AudioProcessor(AudioBuffer& buffer) : audioBuffer(buffer), audioStream(&audioBuffer), wsClient() {}

    void process() {
        audioStream.startStream();

        while (!audioBuffer.empty()) {
            wsClient.sendAudioData(audioBuffer.get());
            audioBuffer.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    AudioBuffer& audioBuffer;
    AudioStream audioStream;
    WebSocketClient wsClient;
};
/*
int main() {
    AudioBuffer audioBuffer;
    AudioProcessor processor(audioBuffer);
    processor.process();
    return 0;
}
*/

