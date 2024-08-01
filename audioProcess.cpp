#include <iostream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <portaudio.h>
#include "config.h"

/* definition of the callback */
static int audioCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
    AudioBuffer* audioBuffer = (AudioBuffer*)userData;
    const float* input = (const float*)inputBuffer;
    audioBuffer->insert(audioBuffer->end(), input, input + framesPerBuffer);
    return paContinue;
}

/* Mainfunction of audio capture- send -get response */
void audioProcess( AudioBuffer inputBuffer) {

    PaError err;
    int deviceNUM = 0;
    PaStreamParameters inputParameters;
   // AudioBuffer audioBuffer;

    AudioBuffer* audioBuffer = &inputBuffer;
    PaStream* stream;
    err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Pa_Initialize error: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    deviceNUM = Pa_GetDeviceCount();
    if (deviceNUM < 0) {
        std::cerr << "GetDeviceCount error: " << Pa_GetErrorText(deviceNUM) << std::endl;
        return;
    }
    //std::cout << "DeviceCount:" << deviceNUM << std::endl;
    //select relavant devices
    if (USE_DEVICE_NUM <= deviceNUM) {
        inputParameters.device = USE_DEVICE_NUM;
    }
    else {
        inputParameters.device = Pa_GetDefaultInputDevice(); // use default device
    }
    //inputParameters.device = Pa_GetDefaultInputDevice();
    //std::cout << "DefaultInputDevice:" << inputParameters.device << std::endl;
    inputParameters.channelCount = 1; // single 
    inputParameters.sampleFormat = paFloat32; // float sample
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        &inputParameters,        //input_parameter 
        NULL,                    // no output
        SAMPLE_RATE,             
        FRAMES_PER_BUFFER,         
        paClipOff,                  
        audioCallback,           // input callback 
        &inputBuffer             // user data
    );

    if (err != paNoError) {
        std::cerr << "Openstream error: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Startstream error: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    // config WebSocket
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver(ioc);
    boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(WEBSOCKET_HOST, WEBSOCKET_PORT);
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws(ioc);

    boost::asio::connect(ws.next_layer(), endpoints);
    ws.handshake(WEBSOCKET_HOST_PORT, "/");

    std::ofstream log("host.log");
    // send audio data
    while (!inputBuffer.empty()) {
        ws.write(boost::asio::buffer(inputBuffer));
        // read_response
        boost::beast::flat_buffer buffer;

        try {
            // read data
            ws.read(buffer);
        }
        catch (const boost::beast::system_error& se) {
            std::cerr << "Boost.Beast error: " << se.code().message() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Standard error: " << e.what() << std::endl;
        }
        //ws.read(buffer);

        std::string hostResponse = boost::beast::buffers_to_string(buffer.data());
        log << hostResponse << std::endl; // write_response to file

        inputBuffer.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }    
    

    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "StopStream error: " << Pa_GetErrorText(err) << std::endl;
    }

    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        std::cerr << "CloseStream error: " << Pa_GetErrorText(err) << std::endl;
    }

    Pa_Terminate();
    ws.close(boost::beast::websocket::close_code::normal);
    log.close();
}

