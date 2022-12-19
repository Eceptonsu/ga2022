/*
 * Simplified Version of Simple-SDL2-Audio by Jake Besworth
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#include "audio.h"

#define AUDIO_FORMAT AUDIO_S16LSB

/* Frequency of the file */
#define AUDIO_FREQUENCY 48000

/* 1 mono, 2 stereo, 4 quad, 6 (5.1) */
#define AUDIO_CHANNELS 2

/* Specifies a unit of audio data to be used at a time. Must be a power of 2 */
#define AUDIO_SAMPLES 4096

/* Max number of sounds that can be in the audio queue at anytime, stops too much mixing */
#define AUDIO_MAX_SOUNDS 25

/* The rate at which the volume fades when musics transition. The higher number indicates music fading faster */
#define AUDIO_MUSIC_FADE_VALUE 2

typedef struct privateAudioDevice
{
    SDL_AudioDeviceID device;
    SDL_AudioSpec want;
    uint8_t audioEnabled;
} PrivateAudioDevice;

/* File scope variables to persist data */
static PrivateAudioDevice* gDevice;
static uint32_t gSoundCount;

static void addMusic(Audio* root, Audio* newAudio);

static inline void playAudio(const char* filename, Audio* audio, uint8_t loop, int volume);

static void addAudio(Audio* root, Audio* newAudio);

static inline void audioCallback(void* userdata, uint8_t* stream, int len);

void playMusic(const char* filename, int volume)
{
    playAudio(filename, NULL, 1, volume);
}

void playMusicFromMemory(Audio* audio, int volume)
{
    playAudio(NULL, audio, 1, volume);
}

void initAudio(void)
{
    Audio* global;
    gDevice = (PrivateAudioDevice*)calloc(1, sizeof(PrivateAudioDevice));
    gSoundCount = 0;

    if (gDevice == NULL)
    {
        fprintf(stderr, "[%s: %d]Fatal Error: Memory c-allocation error\n", __FILE__, __LINE__);
        return;
    }

    gDevice->audioEnabled = 0;

    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO))
    {
        fprintf(stderr, "[%s: %d]Error: SDL_INIT_AUDIO not initialized\n", __FILE__, __LINE__);
        return;
    }

    SDL_memset(&(gDevice->want), 0, sizeof(gDevice->want));

    (gDevice->want).freq = AUDIO_FREQUENCY;
    (gDevice->want).format = AUDIO_FORMAT;
    (gDevice->want).channels = AUDIO_CHANNELS;
    (gDevice->want).samples = AUDIO_SAMPLES;
    (gDevice->want).callback = audioCallback;
    (gDevice->want).userdata = calloc(1, sizeof(Audio));

    global = (Audio*)(gDevice->want).userdata;

    if (global == NULL)
    {
        fprintf(stderr, "[%s: %d]Error: Memory allocation error\n", __FILE__, __LINE__);
        return;
    }

    global->buffer = NULL;
    global->next = NULL;

    /* want.userdata = newAudio; */
    if ((gDevice->device = SDL_OpenAudioDevice(NULL, 0, &(gDevice->want), NULL, 0)) == 0)
    {
        fprintf(stderr, "[%s: %d]Warning: failed to open audio device: %s\n", __FILE__, __LINE__, SDL_GetError());
    }
    else
    {
        /* Set audio device enabled global flag */
        gDevice->audioEnabled = 1;

        /* Unpause active audio stream */
        unpauseAudio();
    }
}

void endAudio(void)
{
    if (gDevice->audioEnabled)
    {
        pauseAudio();

        freeAudio((Audio*)(gDevice->want).userdata);

        /* Close down audio */
        SDL_CloseAudioDevice(gDevice->device);
    }

    free(gDevice);
}

void pauseAudio(void)
{
    if (gDevice->audioEnabled)
    {
        SDL_PauseAudioDevice(gDevice->device, 1);
    }
}

void unpauseAudio(void)
{
    if (gDevice->audioEnabled)
    {
        SDL_PauseAudioDevice(gDevice->device, 0);
    }
}

void freeAudio(Audio* audio)
{
    Audio* temp;

    while (audio != NULL)
    {
        if (audio->free == 1)
        {
            SDL_FreeWAV(audio->bufferTrue);
        }

        temp = audio;
        audio = audio->next;

        free(temp);
    }
}

Audio* createAudio(const char* filename, uint8_t loop, int volume)
{
    Audio* newAudio = (Audio*)calloc(1, sizeof(Audio));

    if (newAudio == NULL)
    {
        fprintf(stderr, "[%s: %d]Error: Memory allocation error\n", __FILE__, __LINE__);
        return NULL;
    }

    if (filename == NULL)
    {
        fprintf(stderr, "[%s: %d]Warning: filename: NULL\n", __FILE__, __LINE__);
        return NULL;
    }

    newAudio->next = NULL;
    newAudio->loop = loop;
    newAudio->fade = 0;
    newAudio->free = 1;
    newAudio->volume = volume;

    if (SDL_LoadWAV(filename, &(newAudio->audio), &(newAudio->bufferTrue), &(newAudio->lengthTrue)) == NULL)
    {
        fprintf(stderr, "[%s: %d]Warning: failed to open wave file: %s error: %s\n", __FILE__, __LINE__, filename, SDL_GetError());
        free(newAudio);
        return NULL;
    }

    newAudio->buffer = newAudio->bufferTrue;
    newAudio->length = newAudio->lengthTrue;
    (newAudio->audio).callback = NULL;
    (newAudio->audio).userdata = NULL;

    return newAudio;
}

static inline void playAudio(const char* filename, Audio* audio, uint8_t loop, int volume)
{
    Audio* newAudio;

    /* Check if audio is enabled */
    if (!gDevice->audioEnabled)
    {
        return;
    }

    /* If sound, check if under max number of sounds allowed, else don't play */
    if (loop == 0)
    {
        if (gSoundCount >= AUDIO_MAX_SOUNDS)
        {
            return;
        }
        else
        {
            gSoundCount++;
        }
    }

    /* Load from filename or from Memory */
    if (filename != NULL)
    {
        /* Create new music sound with loop */
        newAudio = createAudio(filename, loop, volume);
    }
    else if (audio != NULL)
    {
        newAudio = (Audio*)malloc(sizeof(Audio));

        if (newAudio == NULL)
        {
            fprintf(stderr, "[%s: %d]Fatal Error: Memory allocation error\n", __FILE__, __LINE__);
            return;
        }

        memcpy(newAudio, audio, sizeof(Audio));

        newAudio->volume = volume;
        newAudio->loop = loop;
        newAudio->free = 0;
    }
    else
    {
        fprintf(stderr, "[%s: %d]Warning: filename and Audio parameters NULL\n", __FILE__, __LINE__);
        return;
    }

    /* Lock callback function */
    SDL_LockAudioDevice(gDevice->device);

    if (loop == 1)
    {
        addMusic((Audio*)(gDevice->want).userdata, newAudio);
    }
    else
    {
        addAudio((Audio*)(gDevice->want).userdata, newAudio);
    }

    SDL_UnlockAudioDevice(gDevice->device);

}

static void addMusic(Audio* root, Audio* newAudio)
{
    uint8_t musicFound = 0;
    Audio* rootNext = root->next;

    /* Find any existing musics, 0, 1 or 2 and fade them out */
    while (rootNext != NULL)
    {
        /* Phase out any current music */
        if (rootNext->loop == 1 && rootNext->fade == 0)
        {
            if (musicFound)
            {
                rootNext->length = 0;
                rootNext->volume = 0;
            }

            rootNext->fade = 1;
        }
        /* Set flag to remove any queued up music in favour of new music */
        else if (rootNext->loop == 1 && rootNext->fade == 1)
        {
            musicFound = 1;
        }

        rootNext = rootNext->next;
    }

    addAudio(root, newAudio);
}

static inline void audioCallback(void* userdata, uint8_t* stream, int len)
{
    Audio* audio = (Audio*)userdata;
    Audio* previous = audio;
    int tempLength;
    uint8_t music = 0;

    /* Silence the main buffer */
    SDL_memset(stream, 0, len);

    /* First one is place holder */
    audio = audio->next;

    while (audio != NULL)
    {
        if (audio->length > 0)
        {
            if (audio->fade == 1 && audio->loop == 1)
            {
                music = 1;

                if (audio->volume > 0)
                {
                    if (audio->volume - AUDIO_MUSIC_FADE_VALUE < 0)
                    {
                        audio->volume = 0;
                    }
                    else
                    {
                        audio->volume -= AUDIO_MUSIC_FADE_VALUE;
                    }
                }
                else
                {
                    audio->length = 0;
                }
            }

            if (music && audio->loop == 1 && audio->fade == 0)
            {
                tempLength = 0;
            }
            else
            {
                tempLength = ((uint32_t)len > audio->length) ? audio->length : (uint32_t)len;
            }

            SDL_MixAudioFormat(stream, audio->buffer, AUDIO_FORMAT, tempLength, audio->volume);

            audio->buffer += tempLength;
            audio->length -= tempLength;

            previous = audio;
            audio = audio->next;
        }
        else if (audio->loop == 1 && audio->fade == 0)
        {
            audio->buffer = audio->bufferTrue;
            audio->length = audio->lengthTrue;
        }
        else
        {
            previous->next = audio->next;

            if (audio->loop == 0)
            {
                gSoundCount--;
            }

            audio->next = NULL;
            freeAudio(audio);

            audio = previous->next;
        }
    }
}

static void addAudio(Audio* root, Audio* newAudio)
{
    if (root == NULL)
    {
        return;
    }

    while (root->next != NULL)
    {
        root = root->next;
    }

    root->next = newAudio;
}