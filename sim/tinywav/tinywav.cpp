/**
 * Copyright (c) 2015-2017, Martin Roth (mhroth@gmail.com)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */



#include <assert.h>
#include <string.h>
#if _WIN32
#include <winsock2.h>
#include <malloc.h>
#else
#include <alloca.h>
#include <netinet/in.h>
#endif
#include "tinywav.hpp"

int tinywav_open_write(TinyWav *tw,
                       int16_t numChannels, int32_t samplerate,
                       TinyWavSampleFormat sampFmt, TinyWavChannelFormat chanFmt,
                       const char *path) {
#if _WIN32
    errno_t err = fopen_s(&tw->f, path, "w");
    assert(err == 0);
#else
    tw->f = fopen(path, "w");
#endif
    if (!tw->f) {
        return 1;
    }

    tw->numChannels = numChannels;
    tw->totalFramesWritten = 0;
    tw->sampFmt = sampFmt;
    tw->chanFmt = chanFmt;

    // prepare WAV header
    TinyWavHeader h;
    h.ChunkID = htonl(0x52494646); // "RIFF"
    h.ChunkSize = 0; // fill this in on file-close
    h.Format = htonl(0x57415645); // "WAVE"
    h.Subchunk1ID = htonl(0x666d7420); // "fmt "
    h.Subchunk1Size = 16; // PCM
    h.AudioFormat = (tw->sampFmt-1); // 1 PCM, 3 IEEE float
    h.NumChannels = numChannels;
    h.SampleRate = samplerate;
    h.ByteRate = samplerate * numChannels * tw->sampFmt;
    h.BlockAlign = numChannels * tw->sampFmt;
    h.BitsPerSample = 8*tw->sampFmt;
    h.Subchunk2ID = htonl(0x64617461); // "data"
    h.Subchunk2Size = 0; // fill this in on file-close

    // write WAV header
    fwrite(&h, sizeof(TinyWavHeader), 1, tw->f);

    return 0;
}

int tinywav_open_read(TinyWav *tw, const char *path, TinyWavChannelFormat chanFmt, TinyWavSampleFormat sampFmt) {
    tw->f = fopen(path, "rb");
    if (!tw->f) {
        return 1;
    }

    size_t ret = fread(&tw->h, sizeof(TinyWavHeader), 1, tw->f);
    assert(ret > 0);
    assert(tw->h.ChunkID == htonl(0x52494646));        // "RIFF"
    assert(tw->h.Format == htonl(0x57415645));         // "WAVE"
    assert(tw->h.Subchunk1ID == htonl(0x666d7420));    // "fmt "

    // skip over any other chunks before the "data" chunk
    while (tw->h.Subchunk2ID != htonl(0x64617461)) {   // "data"
        // bugfix: if the WAV header contains any extra chunks, like LIST, it was not parsed properly
        fseek(tw->f, tw->h.Subchunk2Size, SEEK_CUR);
        fread(&tw->h.Subchunk2ID, 4, 1, tw->f);
        fread(&tw->h.Subchunk2Size, 4, 1, tw->f);
    }

    tw->numChannels = tw->h.NumChannels;
    tw->chanFmt = chanFmt;
    tw->sampFmt = sampFmt;

    tw->totalFramesWritten = tw->h.Subchunk2Size / (tw->numChannels * tw->sampFmt);
    return 0;
}

int tinywav_read_f(TinyWav *tw, void *data, int len) { // returns number of frames read
    switch (tw->sampFmt) {
        case TW_FLOAT32:
            // nope
            return 0;
        case TW_INT16: {
            size_t samples_read = 0;
            int16_t *interleaved_data = (int16_t *) alloca(tw->numChannels*len*sizeof(int16_t));
            samples_read = fread(interleaved_data, sizeof(int16_t), tw->numChannels*len, tw->f);
            switch (tw->chanFmt) {
                case TW_INTERLEAVED: { // channel buffer is interleaved e.g. [LRLRLRLR]
                    memcpy(data, interleaved_data, tw->numChannels*len*sizeof(int16_t));
                    return (int) (samples_read/tw->numChannels);
                }
                case TW_INLINE: { // channel buffer is inlined e.g. [LLLLRRRR]
                    for (int i = 0, pos = 0; i < tw->numChannels; i++) {
                        for (int j = i; j < len * tw->numChannels; j += tw->numChannels, ++pos) {
                            ((int16_t *) data)[pos] = interleaved_data[j];
                        }
                    }
                    return (int) (samples_read/tw->numChannels);
                }
                case TW_SPLIT: { // channel buffer is split e.g. [[LLLL],[RRRR]]
                    for (int i = 0, pos = 0; i < tw->numChannels; i++) {
                        for (int j = 0; j < len; j++, ++pos) {
                            ((int16_t **) data)[i][j] = interleaved_data[j*tw->numChannels + i];
                        }
                    }
                    return (int) (samples_read/tw->numChannels);
                }
                default: return 0;
            }
        }
        default: return 0;
    }

    return len;
}

void tinywav_close_read(TinyWav *tw) {
    fclose(tw->f);
    tw->f = NULL;
}

size_t tinywav_write_f(TinyWav *tw, int16_t *f, int len) {
    switch (tw->sampFmt) {
        case TW_INT16: {
            int16_t *z = (int16_t *) alloca(tw->numChannels*len*sizeof(int16_t));
            switch (tw->chanFmt) {
                case TW_INTERLEAVED: {
                    for (int i = 0; i < tw->numChannels*len; ++i) {
                        z[i] = f[i];
                    }
                    break;
                }
                case TW_INLINE: {
                    for (int i = 0, k = 0; i < len; ++i) {
                        for (int j = 0; j < tw->numChannels; ++j) {
                            z[k++] = f[j*len+i];
                        }
                    }
                    break;
                }
                case TW_SPLIT: {
                    const int16_t **const x = (const int16_t **const) f;
                    for (int i = 0, k = 0; i < len; ++i) {
                        for (int j = 0; j < tw->numChannels; ++j) {
                            z[k++] = x[j][i];
                        }
                    }
                    break;
                }
                default: return 0;
            }

            tw->totalFramesWritten += len;
            return fwrite(z, sizeof(int16_t), tw->numChannels*len, tw->f);
        }
        case TW_FLOAT32: default:
            return 0;
    }
}

void tinywav_close_write(TinyWav *tw) {
    uint32_t data_len = tw->totalFramesWritten * tw->numChannels * tw->sampFmt;
    
    // set length of data
    fseek(tw->f, 4, SEEK_SET);
    uint32_t chunkSize_len = 36 + data_len;
    fwrite(&chunkSize_len, sizeof(uint32_t), 1, tw->f);

    // Subchunk2Size setting
    fseek(tw->f, 40, SEEK_SET);
    fwrite(&data_len, sizeof(uint32_t), 1, tw->f);

    fclose(tw->f);
    tw->f = NULL;
}

bool tinywav_isOpen(TinyWav *tw) {
    return (tw->f != NULL);
}
