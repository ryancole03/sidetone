#define INITGUID
#include <initguid.h>
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <propkey.h>
#include <propsys.h>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <shlobj.h>
#include <propkey.h>
#include <propsys.h>

#include <speex/speex_preprocess.h>

#include <math.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef _MSC_VER
#error This build currently expects MSVC/Windows SDK headers.
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NATIVELIB_EXPORTS
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __declspec(dllimport)
#endif

DEFINE_GUID(IID_IMMDeviceEnumerator,
    0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(CLSID_MMDeviceEnumerator,
    0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IAudioClient,
    0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioClient2,
    0x726778CD, 0xF60A, 0x4EDA, 0x82, 0xDE, 0xE4, 0x76, 0x10, 0xCD, 0x78, 0xAA);
DEFINE_GUID(IID_IAudioRenderClient,
    0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);
DEFINE_GUID(IID_IAudioCaptureClient,
    0xC8ADBD64, 0xE71E, 0x48A0, 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM,
    0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
    0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);

#define FRAMES 32
#define MAX_CHANNELS 8
#define QUEUE_FRAMES 8192
#define RNNOISE_SAMPLE_QUEUE 8192
#define BUFFER_FRAMES 128

typedef struct {
    int gainPercent;
    int suppressionPercent;
    int inputGainPercent;
    int noiseGateThreshold;
    int bufferSize;
    volatile LONG active;
    wchar_t inputDeviceId[256];
    wchar_t outputDeviceId[256];
    CRITICAL_SECTION cs;
} State;

static State gState = { 25, 75, 100, 10, 128, 1, {0}, {0}, {0} };
static HANDLE gExitEvent = NULL;
static HANDLE gStartEvent = NULL;
static float gFrameQueue[QUEUE_FRAMES * MAX_CHANNELS];
static UINT32 gQueueRead = 0;
static UINT32 gQueueWrite = 0;
static UINT32 gQueueCount = 0;

typedef enum {
    SAMPLE_UNKNOWN = 0,
    SAMPLE_PCM16,
    SAMPLE_PCM32,
    SAMPLE_FLOAT32
} SampleFormat;

static SampleFormat get_sample_format(const WAVEFORMATEX *fmt)
{
    if (!fmt) return SAMPLE_UNKNOWN;

    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
        fmt->wBitsPerSample == 32) {
        return SAMPLE_FLOAT32;
    }

    if (fmt->wFormatTag == WAVE_FORMAT_PCM &&
        fmt->wBitsPerSample == 16) {
        return SAMPLE_PCM16;
    }

    if (fmt->wFormatTag == WAVE_FORMAT_PCM &&
        fmt->wBitsPerSample == 32) {
        return SAMPLE_PCM32;
    }

    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        fmt->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE*)fmt;

        if (IsEqualGUID(&ext->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) &&
            fmt->wBitsPerSample == 32) {
            return SAMPLE_FLOAT32;
        }

        if (IsEqualGUID(&ext->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM) &&
            fmt->wBitsPerSample == 32) {
            return SAMPLE_PCM32;
        }

        if (IsEqualGUID(&ext->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM) &&
            fmt->wBitsPerSample == 16) {
            return SAMPLE_PCM16;
        }
    }

    return SAMPLE_UNKNOWN;
}

static float read_sample_as_float(const BYTE *data, UINT32 sampleIndex, SampleFormat format)
{
    if (format == SAMPLE_FLOAT32) {
        const float *samples = (const float*)(const void*)data;
        return samples[sampleIndex];
    }

    if (format == SAMPLE_PCM16) {
        const int16_t *samples = (const int16_t*)(const void*)data;
        return samples[sampleIndex] / 32768.0f;
    }

    if (format == SAMPLE_PCM32) {
        const int32_t *samples = (const int32_t*)(const void*)data;
        return samples[sampleIndex] / 2147483648.0f;
    }

    return 0.0f;
}

static float clamp_sample(float sample)
{
    if (sample > 1.0f) return 1.0f;
    if (sample < -1.0f) return -1.0f;
    return sample;
}

static void write_sample(BYTE *data, UINT32 sampleIndex, SampleFormat format, float sample)
{
    sample = clamp_sample(sample);

    if (format == SAMPLE_FLOAT32) {
        float *samples = (float*)(void*)data;
        samples[sampleIndex] = sample;
        return;
    }

    if (format == SAMPLE_PCM16) {
        int16_t *samples = (int16_t*)(void*)data;
        samples[sampleIndex] = (int16_t)(sample * 32767.0f);
        return;
    }

    if (format == SAMPLE_PCM32) {
        int32_t *samples = (int32_t*)(void*)data;
        samples[sampleIndex] = (int32_t)(sample * 2147483647.0f);
    }
}

static void queue_push_frame(const float *frame, UINT32 channels)
{
    if (channels == 0 || channels > MAX_CHANNELS)
        return;

    if (gQueueCount == QUEUE_FRAMES) {
        gQueueRead = (gQueueRead + 1) % QUEUE_FRAMES;
        gQueueCount--;
    }

    memcpy(&gFrameQueue[gQueueWrite * channels], frame, channels * sizeof(float));
    gQueueWrite = (gQueueWrite + 1) % QUEUE_FRAMES;
    gQueueCount++;
}

static int queue_pop_frame(float *frame, UINT32 channels)
{
    if (channels == 0 || channels > MAX_CHANNELS || gQueueCount == 0)
        return 0;

    memcpy(frame, &gFrameQueue[gQueueRead * channels], channels * sizeof(float));
    gQueueRead = (gQueueRead + 1) % QUEUE_FRAMES;
    gQueueCount--;
    return 1;
}

static void GetSettingsPath(wchar_t *path, size_t pathSize)
{
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        wsprintfW(path, L"%s\\sidetone.ini", appDataPath);
    } else {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        wchar_t *lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *lastSlash = L'\0';
        wsprintfW(path, L"%s\\sidetone.ini", exePath);
    }
}

void Sidetone_LoadSettings(void)
{
    wchar_t path[MAX_PATH];
    GetSettingsPath(path, MAX_PATH);
    
    int gainI = GetPrivateProfileIntW(L"Settings", L"gain", 25, path);
    int suppressI = GetPrivateProfileIntW(L"Settings", L"suppression", 75, path);
    int inputGainI = GetPrivateProfileIntW(L"Settings", L"inputGain", 100, path);
    int noiseGateI = GetPrivateProfileIntW(L"Settings", L"noiseGate", 10, path);
    int bufferSizeI = GetPrivateProfileIntW(L"Settings", L"bufferSize", 256, path);
    int activeI = GetPrivateProfileIntW(L"Settings", L"active", 1, path);
    
    EnterCriticalSection(&gState.cs);
    gState.gainPercent = gainI;
    gState.suppressionPercent = suppressI;
    gState.inputGainPercent = inputGainI;
    gState.noiseGateThreshold = noiseGateI;
    gState.bufferSize = bufferSizeI;
    GetPrivateProfileStringW(L"Settings", L"inputDevice", L"", gState.inputDeviceId, 256, path);
    GetPrivateProfileStringW(L"Settings", L"outputDevice", L"", gState.outputDeviceId, 256, path);
    LeaveCriticalSection(&gState.cs);
    InterlockedExchange(&gState.active, activeI);
}

void Sidetone_SaveSettings(void)
{
    wchar_t path[MAX_PATH];
    GetSettingsPath(path, MAX_PATH);
    
    wchar_t buf[64];
    
    EnterCriticalSection(&gState.cs);
    wsprintfW(buf, L"%d", gState.gainPercent);
    WritePrivateProfileStringW(L"Settings", L"gain", buf, path);
    
    wsprintfW(buf, L"%d", gState.suppressionPercent);
    WritePrivateProfileStringW(L"Settings", L"suppression", buf, path);
    
    wsprintfW(buf, L"%d", gState.inputGainPercent);
    WritePrivateProfileStringW(L"Settings", L"inputGain", buf, path);

    wsprintfW(buf, L"%d", gState.noiseGateThreshold);
    WritePrivateProfileStringW(L"Settings", L"noiseGate", buf, path);

    wsprintfW(buf, L"%d", gState.bufferSize);
    WritePrivateProfileStringW(L"Settings", L"bufferSize", buf, path);
    
    wsprintfW(buf, L"%d", gState.active);
    WritePrivateProfileStringW(L"Settings", L"active", buf, path);

    if (gState.inputDeviceId[0] != L'\0')
        WritePrivateProfileStringW(L"Settings", L"inputDevice", gState.inputDeviceId, path);
    if (gState.outputDeviceId[0] != L'\0')
        WritePrivateProfileStringW(L"Settings", L"outputDevice", gState.outputDeviceId, path);

    LeaveCriticalSection(&gState.cs);
}

DWORD WINAPI audio_thread(LPVOID arg)
{
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice *inputDevice = NULL;
    IMMDevice *outputDevice = NULL;

    IAudioClient *captureClient = NULL;
    IAudioClient2 *captureClient2 = NULL;
    IAudioClient *renderClient = NULL;

    IAudioCaptureClient *capture = NULL;
    IAudioRenderClient *render = NULL;

    WAVEFORMATEX *captureFormat = NULL;
    WAVEFORMATEX *renderFormat = NULL;
    SampleFormat captureSampleFormat = SAMPLE_UNKNOWN;
    SampleFormat renderSampleFormat = SAMPLE_UNKNOWN;
    UINT32 captureChannels = 0;
    UINT32 renderChannels = 0;
    SpeexPreprocessState *speex = NULL;
    int speexFrameSize = 256;
    float speexInput[256];
    float speexOutput[256];
    int speexIdx = 0;
    int speexOutputReady = 0;
    int speexVadResult = 0;

    float gateGain = 1.0f;
    int gateHoldCounter = 0;

    float levelHistory[12] = {0.0f};
    int historyIndex = 0;

    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hEvent) return 1;

    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        &IID_IMMDeviceEnumerator,
        (void**)&enumerator
    );
    if (FAILED(hr)) return 1;

    State *s = (State*)arg;
    wchar_t inputId[256] = {0};
    wchar_t outputId[256] = {0};

    EnterCriticalSection(&s->cs);
    wcsncpy_s(inputId, 256, s->inputDeviceId, _TRUNCATE);
    wcsncpy_s(outputId, 256, s->outputDeviceId, _TRUNCATE);
    int bufferFrames = s->bufferSize;
    LeaveCriticalSection(&s->cs);

    if (inputId[0] != L'\0') {
        hr = enumerator->lpVtbl->GetDevice(enumerator, inputId, &inputDevice);
        if (FAILED(hr)) inputDevice = NULL;
    }
    if (!inputDevice) {
        hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eCapture, eConsole, &inputDevice);
    }

    if (outputId[0] != L'\0') {
        hr = enumerator->lpVtbl->GetDevice(enumerator, outputId, &outputDevice);
        if (FAILED(hr)) outputDevice = NULL;
    }
    if (!outputDevice) {
        hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &outputDevice);
    }

    if (!inputDevice || !outputDevice) return 1;

    hr = inputDevice->lpVtbl->Activate(
        inputDevice, &IID_IAudioClient2, CLSCTX_ALL, NULL, (void**)&captureClient2
    );
    if (FAILED(hr) || !captureClient2) return 1;

    captureClient = (IAudioClient*)captureClient2;

    {
        AudioClientProperties props = {0};
        props.cbSize = sizeof(props);
        props.bIsOffload = FALSE;
        props.eCategory = AudioCategory_Communications;
        props.Options = AUDCLNT_STREAMOPTIONS_NONE;
        captureClient2->lpVtbl->SetClientProperties(captureClient2, &props);
    }

    hr = captureClient->lpVtbl->GetMixFormat(captureClient, &captureFormat);
    if (FAILED(hr) || !captureFormat) return 1;

    captureSampleFormat = get_sample_format(captureFormat);
    captureChannels = captureFormat->nChannels;

    if (captureSampleFormat == SAMPLE_UNKNOWN || captureChannels == 0 || captureChannels > MAX_CHANNELS)
        return 1;

    hr = captureClient->lpVtbl->Initialize(
        captureClient,
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferFrames * 10000000 / captureFormat->nSamplesPerSec,
        0,
        captureFormat,
        NULL
    );
    if (FAILED(hr)) {
        hr = captureClient->lpVtbl->Initialize(
            captureClient,
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            0, 0,
            captureFormat,
            NULL
        );
    }
    if (FAILED(hr)) return 1;

    hr = captureClient->lpVtbl->SetEventHandle(captureClient, hEvent);
    if (FAILED(hr)) return 1;

    hr = captureClient->lpVtbl->GetService(
        captureClient, &IID_IAudioCaptureClient, (void**)&capture
    );
    if (FAILED(hr) || !capture) return 1;

    hr = outputDevice->lpVtbl->Activate(
        outputDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&renderClient
    );
    if (FAILED(hr) || !renderClient) return 1;

    hr = renderClient->lpVtbl->GetMixFormat(renderClient, &renderFormat);
    if (FAILED(hr) || !renderFormat) return 1;

    renderSampleFormat = get_sample_format(renderFormat);
    renderChannels = renderFormat->nChannels;

    if (renderSampleFormat == SAMPLE_UNKNOWN ||
        renderChannels == 0 || renderChannels > MAX_CHANNELS) {
        return 1;
    }
    
    speex = speex_preprocess_state_init(speexFrameSize, captureFormat->nSamplesPerSec);
    if (speex) {
        int denoise = 1;
        int noiseSuppress = -40;
        int vad = 0;
        int agc = 0;
        int dereverb = 0;
        int probStart = 80;
        int probContinue = 65;
        
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_VAD, &vad);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_AGC, &agc);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_DEREVERB, &dereverb);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_PROB_START, &probStart);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_PROB_CONTINUE, &probContinue);
    }

    hr = renderClient->lpVtbl->Initialize(
        renderClient,
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferFrames * 10000000 / renderFormat->nSamplesPerSec,
        0,
        renderFormat,
        NULL
    );
    if (FAILED(hr)) return 1;

    hr = renderClient->lpVtbl->GetService(
        renderClient, &IID_IAudioRenderClient, (void**)&render
    );
    if (FAILED(hr) || !render) return 1;

    hr = captureClient->lpVtbl->Start(captureClient);
    if (FAILED(hr)) return 1;

    hr = renderClient->lpVtbl->Start(renderClient);
    if (FAILED(hr)) return 1;

    SetEvent(gStartEvent);

    while (1)
    {
        HANDLE handles[2] = { hEvent, gExitEvent };
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, 10);

        if (waitResult == WAIT_OBJECT_0 + 1) {
            break;
        }

        UINT32 packetLength = 0;
        capture->lpVtbl->GetNextPacketSize(capture, &packetLength);

        while (packetLength > 0)
        {
            BYTE *data = NULL;
            UINT32 frames = 0;
            DWORD flags = 0;

            hr = capture->lpVtbl->GetBuffer(
                capture, &data, &frames, &flags, NULL, NULL
            );
            if (FAILED(hr) || !data)
                break;

            UINT32 padding = 0;
            hr = renderClient->lpVtbl->GetCurrentPadding(renderClient, &padding);
            if (FAILED(hr))
            {
                capture->lpVtbl->ReleaseBuffer(capture, frames);
                break;
            }

            UINT32 bufferSize = 0;
            hr = renderClient->lpVtbl->GetBufferSize(renderClient, &bufferSize);
            if (FAILED(hr))
            {
                capture->lpVtbl->ReleaseBuffer(capture, frames);
                break;
            }

            UINT32 available = bufferSize - padding;
            if (available > frames)
                available = frames;

            if (available == 0)
            {
                capture->lpVtbl->ReleaseBuffer(capture, frames);
                capture->lpVtbl->GetNextPacketSize(capture, &packetLength);
                continue;
            }

            BYTE *outData = NULL;
            hr = render->lpVtbl->GetBuffer(render, available, &outData);
            if (FAILED(hr) || !outData)
            {
                capture->lpVtbl->ReleaseBuffer(capture, frames);
                break;
            }

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                memset(data, 0, frames * captureFormat->nBlockAlign);
            }

            for (UINT32 frameIndex = 0; frameIndex < frames; frameIndex++)
            {
                float outFrame[MAX_CHANNELS] = { 0 };
                float drySample = 0.0f;
                float wetSample = 0.0f;

                for (UINT32 channel = 0; channel < renderChannels; channel++)
                {
                    UINT32 sourceChannel = channel;

                    if (captureChannels == 1) {
                        sourceChannel = 0;
                    } else if (channel >= captureChannels) {
                        sourceChannel = captureChannels - 1;
                    }

                    drySample = read_sample_as_float(
                        data,
                        frameIndex * captureChannels + sourceChannel,
                        captureSampleFormat
                    );
                    break;
                }
                
                drySample *= s->inputGainPercent / 100.0f;

                if (speex) {
                    speexInput[speexIdx++] = drySample;
                    
                    if (speexIdx >= speexFrameSize) {
                        spx_int16_t intBuf[256];
                        for (int i = 0; i < speexFrameSize; i++)
                            intBuf[i] = (spx_int16_t)(speexInput[i] * 32767.0f);
                        
                        speex_preprocess_run(speex, intBuf);
                        
                        int vad = 0;
                        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_GET_VAD, &vad);
                        
                        for (int i = 0; i < speexFrameSize; i++)
                            speexOutput[i] = intBuf[i] / 32767.0f;
                        
                        speexVadResult = vad;
                        speexOutputReady = speexFrameSize;
                        speexIdx = 0;
                    }
                    
                    if (speexOutputReady > 0) {
                        wetSample = speexOutput[speexFrameSize - speexOutputReady];
                        speexOutputReady--;
                    } else {
                        wetSample = drySample;
                    }
                } else {
                    wetSample = drySample;
                }

                float inputLevel = wetSample;
                float gatePercent = s->noiseGateThreshold;
                LeaveCriticalSection(&s->cs);

                if (inputLevel < 0) inputLevel = -inputLevel;

                levelHistory[historyIndex] = inputLevel;
                historyIndex = (historyIndex + 1) % 12;
                
                float sumSquares = 0.0f;
                for (int i = 0; i < 12; i++) {
                    sumSquares += levelHistory[i] * levelHistory[i];
                }
                float rmsLevel = sqrtf(sumSquares / 12.0f);

                float targetGain = 1.0f;
                static int gateHoldCounter = 0;
                const int gateHoldFrames = 40;

                if (gatePercent > 0) {
                    float openThreshold = gatePercent * 0.0004f;
                    int noVoice = (speexVadResult == 0);
                    int belowThreshold = (rmsLevel < openThreshold);

                    if (noVoice && belowThreshold) {
                        if (gateHoldCounter < gateHoldFrames) {
                            gateHoldCounter++;
                        } else {
                            targetGain = 0.0f;
                        }
                    } else {
                        if (gateHoldCounter > 0) {
                            gateHoldCounter--;
                        }
                        targetGain = 1.0f;
                    }

                    float attackCoeff = 0.05f;
                    float releaseCoeff = 0.02f;

                    if (targetGain > gateGain) {
                        gateGain += attackCoeff;
                        if (gateGain > targetGain) gateGain = targetGain;
                    } else if (targetGain < gateGain) {
                        gateGain -= releaseCoeff;
                        if (gateGain < targetGain) gateGain = targetGain;
                    }
                } else {
                    gateGain = 1.0f;
                    gateHoldCounter = 0;
                }

                wetSample = wetSample * gateGain;

                LONG isActive = InterlockedOr(&gState.active, 0);

                float gain;
                float suppression;
                EnterCriticalSection(&s->cs);
                gain = s->gainPercent / 100.0f;
                suppression = s->suppressionPercent / 100.0f;
                LeaveCriticalSection(&s->cs);

                for (UINT32 channel = 0; channel < renderChannels; channel++)
                {
                    float sample;

                    if (isActive) {
                        sample = wetSample * gain;
                    } else {
                        sample = 0.0f;
                    }

                    outFrame[channel] = sample;
                }

                static int frameCounter = 0;
                if (speex && ++frameCounter >= 1000) {
                    int noiseSuppress = -15 - (int)(suppression * 20);
                    speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);
                    frameCounter = 0;
                }

                queue_push_frame(outFrame, renderChannels);
            }

            for (UINT32 frameIndex = 0; frameIndex < available; frameIndex++)
            {
                float outFrame[MAX_CHANNELS] = { 0 };

                if (!queue_pop_frame(outFrame, renderChannels))
                    break;

                for (UINT32 channel = 0; channel < renderChannels; channel++)
                {
                    write_sample(
                        outData,
                        frameIndex * renderChannels + channel,
                        renderSampleFormat,
                        outFrame[channel]
                    );
                }
            }

            render->lpVtbl->ReleaseBuffer(render, available, 0);
            capture->lpVtbl->ReleaseBuffer(capture, frames);

            capture->lpVtbl->GetNextPacketSize(capture, &packetLength);
        }
    }

    if (speex) {
        speex_preprocess_state_destroy(speex);
    }

    if (capture) capture->lpVtbl->Release(capture);
    if (render) render->lpVtbl->Release(render);

    if (captureClient) captureClient->lpVtbl->Stop(captureClient);
    if (renderClient) renderClient->lpVtbl->Stop(renderClient);

    if (captureClient) captureClient->lpVtbl->Release(captureClient);
    if (captureClient2) captureClient2->lpVtbl->Release(captureClient2);
    if (renderClient) renderClient->lpVtbl->Release(renderClient);

    if (inputDevice) inputDevice->lpVtbl->Release(inputDevice);
    if (outputDevice) outputDevice->lpVtbl->Release(outputDevice);
    if (enumerator) enumerator->lpVtbl->Release(enumerator);

    if (captureFormat) CoTaskMemFree(captureFormat);
    if (renderFormat) CoTaskMemFree(renderFormat);

    CloseHandle(hEvent);
    CoUninitialize();

    return 0;
}

static HANDLE gAudioThread = NULL;

EXPORT void Sidetone_Init(void)
{
    InitializeCriticalSection(&gState.cs);
    Sidetone_LoadSettings();
}

EXPORT void Sidetone_Shutdown(void)
{
    Sidetone_SaveSettings();
    if (gAudioThread) {
        CloseHandle(gAudioThread);
        gAudioThread = NULL;
    }
    DeleteCriticalSection(&gState.cs);
}

EXPORT int Sidetone_Start(void)
{
    if (gAudioThread) return 1;

    gExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    gStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!gExitEvent || !gStartEvent) return 0;

    gAudioThread = CreateThread(NULL, 0, audio_thread, &gState, 0, NULL);
    if (!gAudioThread) {
        CloseHandle(gExitEvent);
        CloseHandle(gStartEvent);
        return 0;
    }

    WaitForSingleObject(gStartEvent, 5000);
    CloseHandle(gStartEvent);
    return 1;
}

EXPORT void Sidetone_Stop(void)
{
    if (gExitEvent) {
        SetEvent(gExitEvent);
    }
    if (gAudioThread) {
        WaitForSingleObject(gAudioThread, 5000);
        CloseHandle(gAudioThread);
        gAudioThread = NULL;
    }
    if (gExitEvent) {
        CloseHandle(gExitEvent);
        gExitEvent = NULL;
    }
}

EXPORT void Sidetone_SetGainPercent(int percent)
{
    EnterCriticalSection(&gState.cs);
    gState.gainPercent = percent;
    LeaveCriticalSection(&gState.cs);
}

EXPORT void Sidetone_SetSuppressionPercent(int percent)
{
    EnterCriticalSection(&gState.cs);
    gState.suppressionPercent = percent;
    LeaveCriticalSection(&gState.cs);
}

EXPORT void Sidetone_SetInputGainPercent(int percent)
{
    EnterCriticalSection(&gState.cs);
    gState.inputGainPercent = percent;
    LeaveCriticalSection(&gState.cs);
}

EXPORT int Sidetone_IsActive(void)
{
    return InterlockedOr(&gState.active, 0);
}

EXPORT void Sidetone_SetActive(int active)
{
    InterlockedExchange(&gState.active, active ? 1 : 0);
}

EXPORT int Sidetone_GetGainPercent(void)
{
    EnterCriticalSection(&gState.cs);
    int percent = gState.gainPercent;
    LeaveCriticalSection(&gState.cs);
    return percent;
}

EXPORT int Sidetone_GetSuppressionPercent(void)
{
    EnterCriticalSection(&gState.cs);
    int percent = gState.suppressionPercent;
    LeaveCriticalSection(&gState.cs);
    return percent;
}

EXPORT int Sidetone_GetInputGainPercent(void)
{
    EnterCriticalSection(&gState.cs);
    int percent = gState.inputGainPercent;
    LeaveCriticalSection(&gState.cs);
    return percent;
}

EXPORT int Sidetone_GetInputDeviceCount(void)
{
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    int count = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (FAILED(hr) || !enumerator) { CoUninitialize(); return 0; }

    hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator, eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr) && collection) {
        collection->lpVtbl->GetCount(collection, (UINT*)&count);
        collection->lpVtbl->Release(collection);
    }
    enumerator->lpVtbl->Release(enumerator);
    CoUninitialize();
    return count;
}

EXPORT int Sidetone_GetInputDevice(int index, wchar_t *id, wchar_t *name, int bufferSize)
{
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    IMMDevice *device = NULL;
    int result = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (FAILED(hr) || !enumerator) { CoUninitialize(); return 0; }

    hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator, eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr) && collection) {
        UINT count = 0;
        collection->lpVtbl->GetCount(collection, &count);
        if (index < (int)count) {
            hr = collection->lpVtbl->Item(collection, index, &device);
            if (SUCCEEDED(hr) && device) {
                LPWSTR deviceId = NULL;
                hr = device->lpVtbl->GetId(device, &deviceId);
                if (SUCCEEDED(hr) && deviceId && id) {
                    wcsncpy_s(id, bufferSize, deviceId, _TRUNCATE);
                    CoTaskMemFree(deviceId);
                    result = 1;
                }
                IPropertyStore *propStore = NULL;
                hr = device->lpVtbl->OpenPropertyStore(device, STGM_READ, &propStore);
                if (SUCCEEDED(hr) && propStore) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    hr = propStore->lpVtbl->GetValue(propStore, &PKEY_Device_FriendlyName, &varName);
                    if (SUCCEEDED(hr) && varName.pwszVal && name) {
                        wcsncpy_s(name, bufferSize, varName.pwszVal, _TRUNCATE);
                    }
                    PropVariantClear(&varName);
                    propStore->lpVtbl->Release(propStore);
                }
                device->lpVtbl->Release(device);
            }
        }
        collection->lpVtbl->Release(collection);
    }
    enumerator->lpVtbl->Release(enumerator);
    CoUninitialize();
    return result;
}

EXPORT int Sidetone_GetOutputDeviceCount(void)
{
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    int count = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (FAILED(hr) || !enumerator) { CoUninitialize(); return 0; }

    hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator, eRender, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr) && collection) {
        collection->lpVtbl->GetCount(collection, (UINT*)&count);
        collection->lpVtbl->Release(collection);
    }
    enumerator->lpVtbl->Release(enumerator);
    CoUninitialize();
    return count;
}

EXPORT int Sidetone_GetOutputDevice(int index, wchar_t *id, wchar_t *name, int bufferSize)
{
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    IMMDevice *device = NULL;
    int result = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (FAILED(hr) || !enumerator) { CoUninitialize(); return 0; }

    hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator, eRender, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr) && collection) {
        UINT count = 0;
        collection->lpVtbl->GetCount(collection, &count);
        if (index < (int)count) {
            hr = collection->lpVtbl->Item(collection, index, &device);
            if (SUCCEEDED(hr) && device) {
                LPWSTR deviceId = NULL;
                hr = device->lpVtbl->GetId(device, &deviceId);
                if (SUCCEEDED(hr) && deviceId && id) {
                    wcsncpy_s(id, bufferSize, deviceId, _TRUNCATE);
                    CoTaskMemFree(deviceId);
                    result = 1;
                }
                IPropertyStore *propStore = NULL;
                hr = device->lpVtbl->OpenPropertyStore(device, STGM_READ, &propStore);
                if (SUCCEEDED(hr) && propStore) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    hr = propStore->lpVtbl->GetValue(propStore, &PKEY_Device_FriendlyName, &varName);
                    if (SUCCEEDED(hr) && varName.pwszVal && name) {
                        wcsncpy_s(name, bufferSize, varName.pwszVal, _TRUNCATE);
                    }
                    PropVariantClear(&varName);
                    propStore->lpVtbl->Release(propStore);
                }
                device->lpVtbl->Release(device);
            }
        }
        collection->lpVtbl->Release(collection);
    }
    enumerator->lpVtbl->Release(enumerator);
    CoUninitialize();
    return result;
}

EXPORT void Sidetone_SetInputDevice(const wchar_t *deviceId)
{
    EnterCriticalSection(&gState.cs);
    if (deviceId) wcsncpy_s(gState.inputDeviceId, 256, deviceId, _TRUNCATE);
    else gState.inputDeviceId[0] = L'\0';
    LeaveCriticalSection(&gState.cs);
}

EXPORT void Sidetone_SetOutputDevice(const wchar_t *deviceId)
{
    EnterCriticalSection(&gState.cs);
    if (deviceId) wcsncpy_s(gState.outputDeviceId, 256, deviceId, _TRUNCATE);
    else gState.outputDeviceId[0] = L'\0';
    LeaveCriticalSection(&gState.cs);
}

EXPORT void Sidetone_GetCurrentInputDevice(wchar_t *deviceId, int bufferSize)
{
    EnterCriticalSection(&gState.cs);
    wcsncpy_s(deviceId, (rsize_t)bufferSize, gState.inputDeviceId, _TRUNCATE);
    LeaveCriticalSection(&gState.cs);
}

EXPORT void Sidetone_GetCurrentOutputDevice(wchar_t *deviceId, int bufferSize)
{
    EnterCriticalSection(&gState.cs);
    wcsncpy_s(deviceId, (rsize_t)bufferSize, gState.outputDeviceId, _TRUNCATE);
    LeaveCriticalSection(&gState.cs);
}

EXPORT void Sidetone_SetNoiseGateThreshold(int threshold)
{
    EnterCriticalSection(&gState.cs);
    gState.noiseGateThreshold = threshold;
    LeaveCriticalSection(&gState.cs);
}

EXPORT int Sidetone_GetNoiseGateThreshold(void)
{
    EnterCriticalSection(&gState.cs);
    int threshold = gState.noiseGateThreshold;
    LeaveCriticalSection(&gState.cs);
    return threshold;
}

EXPORT void Sidetone_SetBufferSize(int size)
{
    EnterCriticalSection(&gState.cs);
    if (size >= 64 && size <= 1024) {
        gState.bufferSize = size;
    }
    LeaveCriticalSection(&gState.cs);
}

EXPORT int Sidetone_GetBufferSize(void)
{
    EnterCriticalSection(&gState.cs);
    int size = gState.bufferSize;
    LeaveCriticalSection(&gState.cs);
    return size;
}

EXPORT int Sidetone_GetInputSampleRate(void)
{
    int sampleRate = 48000;
    
    HRESULT hr;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice *device = NULL;
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (SUCCEEDED(hr) && enumerator) {
        wchar_t inputId[256] = {0};
        EnterCriticalSection(&gState.cs);
        wcsncpy_s(inputId, 256, gState.inputDeviceId, _TRUNCATE);
        LeaveCriticalSection(&gState.cs);
        
        if (inputId[0] != L'\0') {
            hr = enumerator->lpVtbl->GetDevice(enumerator, inputId, &device);
        }
        if (!device) {
            enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eCapture, eConsole, &device);
        }
        
        if (device) {
            IAudioClient *client = NULL;
            hr = device->lpVtbl->Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);
            if (SUCCEEDED(hr) && client) {
                WAVEFORMATEX *fmt = NULL;
                hr = client->lpVtbl->GetMixFormat(client, &fmt);
                if (SUCCEEDED(hr) && fmt) {
                    sampleRate = fmt->nSamplesPerSec;
                    CoTaskMemFree(fmt);
                }
                client->lpVtbl->Release(client);
            }
            device->lpVtbl->Release(device);
        }
        enumerator->lpVtbl->Release(enumerator);
    }
    
    CoUninitialize();
    return sampleRate;
}

EXPORT int Sidetone_GetOutputSampleRate(void)
{
    int sampleRate = 48000;
    
    HRESULT hr;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice *device = NULL;
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (SUCCEEDED(hr) && enumerator) {
        wchar_t outputId[256] = {0};
        EnterCriticalSection(&gState.cs);
        wcsncpy_s(outputId, 256, gState.outputDeviceId, _TRUNCATE);
        LeaveCriticalSection(&gState.cs);
        
        if (outputId[0] != L'\0') {
            hr = enumerator->lpVtbl->GetDevice(enumerator, outputId, &device);
        }
        if (!device) {
            enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
        }
        
        if (device) {
            IAudioClient *client = NULL;
            hr = device->lpVtbl->Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);
            if (SUCCEEDED(hr) && client) {
                WAVEFORMATEX *fmt = NULL;
                hr = client->lpVtbl->GetMixFormat(client, &fmt);
                if (SUCCEEDED(hr) && fmt) {
                    sampleRate = fmt->nSamplesPerSec;
                    CoTaskMemFree(fmt);
                }
                client->lpVtbl->Release(client);
            }
            device->lpVtbl->Release(device);
        }
        enumerator->lpVtbl->Release(enumerator);
    }
    
    CoUninitialize();
    return sampleRate;
}

#ifdef __cplusplus
}
#endif
