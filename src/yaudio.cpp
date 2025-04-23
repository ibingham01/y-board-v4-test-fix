#include "yaudio.h"

#include <Arduino.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <FS.h>
#include <SD.h>

namespace YAudio {

///////////////////////////////// Configuration Constants //////////////////////

static const int MAX_NOTES_IN_BUFFER = 4000;

// This is the sequence of notes to play
static std::string notes;

// Notes state
static int beats_per_minute;
static int octave;
static int volume_notes;

typedef struct {
    unsigned int frequency;
    unsigned int duration;
} note_t;

// Note playing task
static TaskHandle_t play_speaker_task_handle;
static SemaphoreHandle_t notes_mutex;

// General stream variables
static StreamCopy copier;

// Variables for speaker
static I2SStream speakerOut;
static PoppingSoundRemover<int16_t> poppingRemover(1, true, true);

// Variables for tone generation
static AudioInfo sineInfo(16000, 1, 16);
static SineWaveGenerator<int16_t> sineWave(16000);
static GeneratedSoundStream<int16_t> toneStream(sineWave);
static bool playing_tones = false;

// Variables for audio file decoding
static File sound_file;
static VolumeStream speakerVolume(speakerOut);
static EncodedAudioStream wav_decoder(&speakerVolume, new WAVDecoder());
static EncodedAudioStream mp3_decoder(&speakerVolume, new MP3DecoderHelix());
static bool playing_file = false;

// Variables for microphone
static File speaker_recording_file;
static AudioInfo micInfo(44100, 1, 16);
static I2SStream micIn;
static VolumeStream micVolume(micIn);

// Variables for recording
static EncodedAudioStream wav_encoder(&speaker_recording_file, new WAVEncoder());
static bool recording_audio = false;
static bool done_recording_audio = true;

//////////////////////////// Private Function Prototypes ///////////////////////
// Local private functions
static void play_speaker_task(void *params);
static void recording_audio_task(void *params);
static note_t parse_next_note();
static void set_note_defaults();

////////////////////////////// Public Functions ///////////////////////////////
bool setup_speaker(int ws_pin, int bck_pin, int data_pin, int i2s_port) {
    set_note_defaults();

    Serial.println("starting I2S...");
    auto config = speakerOut.defaultConfig(TX_MODE);
    config.copyFrom(sineInfo);
    config.pin_ws = ws_pin;
    config.pin_bck = bck_pin;
    config.pin_data = data_pin;
    config.port_no = i2s_port;

    speakerOut.begin(config);
    speakerVolume.begin(config);

    // Create the mutex for notes string
    notes_mutex = xSemaphoreCreateMutex();

    // Create task that will actually do the playing
    xTaskCreate(play_speaker_task, "play_speaker_task", 4096, NULL, 1, &play_speaker_task_handle);

    return true;
}

bool setup_mic(int ws_pin, int data_pin, int i2s_port) {
    auto config = micIn.defaultConfig(RX_MODE);
    config.copyFrom(micInfo);
    config.signal_type = PDM;
    config.i2s_format = I2S_STD_FORMAT;
    config.is_master = true;
    config.port_no = i2s_port;
    config.pin_ws = ws_pin;
    config.pin_data = data_pin;

    auto volumeConfig = micVolume.defaultConfig();
    volumeConfig.copyFrom(micInfo);
    volumeConfig.allow_boost = true;

    micIn.begin(config);
    micVolume.begin(volumeConfig);

    return true;
}

bool start_recording(const std::string &filename) {
    if (recording_audio) {
        Serial.println("Already recording audio");
        return false;
    }

    speaker_recording_file = SD.open(filename.c_str(), FILE_WRITE);
    if (!speaker_recording_file) {
        Serial.println("Error opening/creating file for recording.");
        return false;
    }

    // Set up initial state
    recording_audio = true;
    done_recording_audio = false;

    // Create the task to actually do the recording
    xTaskCreate(recording_audio_task, "recording_audio_task", 4096, NULL, 1, NULL);

    return true;
}

void recording_audio_task(void *params) {
    wav_encoder.begin(micInfo);
    copier.begin(wav_encoder, micVolume);

    while (recording_audio) {
        copier.copy();
    }

    speaker_recording_file.flush();
    speaker_recording_file.close();
    wav_encoder.end();

    // Indicate to the main task that we are done
    done_recording_audio = true;

    // This task is done so delete itself
    vTaskDelete(NULL);
}

void stop_recording() {
    // Notify the other task it should be done
    recording_audio = false;

    // Wait for other task to finish
    while (!done_recording_audio) {
        delay(10);
    }
}

bool is_recording() { return recording_audio; }

void set_recording_gain(uint8_t new_gain) { micVolume.setVolume(new_gain); }

I2SStream &get_speaker_stream() { return speakerOut; }

I2SStream &get_mic_stream() { return micIn; }

bool add_notes(const std::string &new_notes) {
    if ((notes.length() + new_notes.length()) > MAX_NOTES_IN_BUFFER) {
        Serial.printf("Error adding notes: too many notes in buffer (%d + %d > %d).\n",
                      new_notes.length(), notes.length(), MAX_NOTES_IN_BUFFER);
        return false;
    }

    // Append the new notes to the existing notes
    xSemaphoreTake(notes_mutex, portMAX_DELAY);
    notes += new_notes;
    xSemaphoreGive(notes_mutex);

    // Signal we need to play the notes
    playing_tones = true;
    xTaskNotifyGive(play_speaker_task_handle);

    return true;
}

void stop_speaker() {
    // Update flags
    playing_tones = false;
    playing_file = false;

    // Clear out all pending notes
    xSemaphoreTake(notes_mutex, portMAX_DELAY);
    notes.clear();
    xSemaphoreGive(notes_mutex);

    copier.end();
}

bool is_playing() { return playing_tones || playing_file; }

bool play_sound_file(const std::string &filename) {
    // Whether notes or wave is running, stop it
    stop_speaker();

    sound_file = SD.open(filename.c_str());
    if (!sound_file) {
        Serial.printf("Error opening file: %s\n", filename.c_str());
        return false;
    }

    char start[4];
    sound_file.readBytes(start, 4);
    sound_file.seek(0);

    if (start[0] == 0xFF || start[0] == 0xFE || strncmp("ID3", (const char *)start, 3) == 0) {
        LOGI("using MP3DecoderHelix");
        mp3_decoder.end();
        mp3_decoder.begin();
        copier.begin(mp3_decoder, sound_file);
    } else if (strncmp("RIFF", (const char *)start, 4) == 0) {
        LOGI("using WAVDecoder");
        wav_decoder.end();
        wav_decoder.begin();
        copier.begin(wav_decoder, sound_file);
    } else {
        LOGE("Unknown file type");
        return false;
    }

    playing_file = true;
    xTaskNotifyGive(play_speaker_task_handle);

    return true;
}

////////////////////////////// Private Functions ///////////////////////////////

void set_note_defaults() {
    beats_per_minute = 120;
    octave = 5;
    volume_notes = 5;
}

note_t parse_next_note() {
    static float note_freq;
    static float duration_s;

    while (notes.length()) {
        // If first character is white space, remove it and continue
        if (isspace(notes[0])) {
            notes.erase(0, 1);
            continue;
        }

        // Octave
        if (notes[0] == 'O' || notes[0] == 'o') {
            int new_octave = notes[1] - '0';
            if (new_octave >= 4 && new_octave <= 7) {
                octave = new_octave;
            }
            notes.erase(0, 2);
            continue;
        }

        // Tempo
        if (notes[0] == 'T' || notes[0] == 't') {
            notes.erase(0, 1);
            size_t pos;
            int new_tempo = std::stoi(notes, &pos);
            notes = notes.substr(pos);
            if (new_tempo >= 40 && new_tempo <= 240) {
                beats_per_minute = new_tempo;
            }
            continue;
        }

        // Reset
        if (notes[0] == '!') {
            set_note_defaults();
            notes.erase(0, 1);
            continue;
        }

        // Volume
        if (notes[0] == 'V' || notes[0] == 'v') {
            notes.erase(0, 1);
            size_t pos;
            int new_volume = std::stoi(notes, &pos);
            notes = notes.substr(pos);
            if (new_volume >= 1 && new_volume <= 10) {
                volume_notes = new_volume;
            }
            continue;
        }

        duration_s = (60.0 / beats_per_minute); // Quarter note duration in seconds

        // A-G regular notes
        // R for rest
        // z for end rest, which is added internally to stop speaker crackle at the end
        if ((notes[0] >= 'A' && notes[0] <= 'G') || (notes[0] >= 'a' && notes[0] <= 'g') ||
            notes[0] == 'R' || notes[0] == 'r' || notes[0] == 'z') {
            switch (notes[0]) {
            case 'A':
            case 'a':
                note_freq = 440.0;
                break;
            case 'B':
            case 'b':
                note_freq = 493.88;
                break;
            case 'C':
            case 'c':
                note_freq = 523.25;
                break;
            case 'D':
            case 'd':
                note_freq = 587.33;
                break;
            case 'E':
            case 'e':
                note_freq = 659.25;
                break;
            case 'F':
            case 'f':
                note_freq = 698.46;
                break;
            case 'G':
            case 'g':
                note_freq = 783.99;
                break;
            case 'z':
                duration_s = 0.2;
                // Fallthrough
            case 'R':
            case 'r':
                note_freq = 0;
                break;
            }

            // Adjust frequency for octave
            note_freq *= pow(2, octave - 4);
            notes.erase(0, 1);

            float dot_duration = duration_s;

            // Note modifiers
            while (1) {

                // Duration
                if (isdigit(notes[0])) {
                    size_t pos;
                    int frac_duration = std::stoi(notes, &pos);
                    notes = notes.substr(pos);
                    if (frac_duration >= 1 && frac_duration <= 2000) {
                        duration_s = duration_s * (4.0 / frac_duration);
                    }
                    continue;
                }

                // Dot
                if (notes[0] == '.') {
                    dot_duration /= 2;
                    duration_s += dot_duration;
                    notes.erase(0, 1);
                    continue;
                }

                // Octave
                if (notes[0] == '>') {
                    note_freq *= 2;
                    notes.erase(0, 1);
                    continue;
                }
                if (notes[0] == '<') {
                    note_freq /= 2;
                    notes.erase(0, 1);
                    continue;
                }

                // Sharp/flat
                if (notes[0] == '#' || notes[0] == '+' || notes[0] == '-') {
                    if (notes[0] == '#' || notes[0] == '+') {
                        note_freq *= pow(2, 1.0 / 12);
                    } else {
                        note_freq /= pow(2, 1.0 / 12);
                    }
                    notes.erase(0, 1);
                    continue;
                }

                break;
            }
            return {(unsigned int)round(note_freq), (unsigned int)(duration_s * 1000)};
        }

        // If we reach here then we have a syntax error
        Serial.printf("Syntax error in notes: %s\n", notes.c_str());
        notes.clear();
        break;
    }

    return {0, 0};
}

void set_wave_volume(uint8_t new_volume) { speakerVolume.setVolume(new_volume / 10.0); }

void play_speaker_task(void *params) {
    while (1) {
        // Block waiting for something to do
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (playing_tones) {
            // Setup sine wave
            sineWave.begin(sineInfo);

            // Start the audio stream
            copier.begin(speakerOut, toneStream);

            // Play all the notes until there are none left
            while (notes.length()) {
                xSemaphoreTake(notes_mutex, portMAX_DELAY);
                note_t note = parse_next_note();
                xSemaphoreGive(notes_mutex);

                // Play the tone and wait for it to finish
                sineWave.setFrequency(note.frequency);
                sineWave.setAmplitude(16000 * (volume_notes / 10.0));

                // Copy during the note duration
                int start_time = millis();
                while ((millis() - start_time) < note.duration) {
                    copier.copy(poppingRemover);
                }
            }

            // If all of the notes have been played, signal that we are done
            playing_tones = false;
        }

        if (playing_file) {
            // Keep copying until the file and copier is done
            while (playing_file &&
                   !(copier.copy(poppingRemover) == 0 && sound_file.available() == 0))
                ;
            playing_file = false;
        }
    }
}
}; // namespace YAudio
