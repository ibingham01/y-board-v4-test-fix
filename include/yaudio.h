#ifndef YAUDIO_H
#define YAUDIO_H

#include <AudioTools.h>
#include <stdint.h>
#include <string>

namespace YAudio {

bool setup_speaker(int ws_pin, int bck_pin, int data_pin, int i2s_port);
bool setup_mic(int ws_pin, int data_pin, int i2s_port);
I2SStream &get_speaker_stream();
I2SStream &get_mic_stream();
void set_wave_volume(uint8_t volume);
bool add_notes(const std::string &new_notes);
void stop_speaker();
bool is_playing();
bool play_sound_file(const std::string &filename);
bool start_recording(const std::string &filename);
void stop_recording();
bool is_recording();
void set_recording_gain(uint8_t new_gain);
}; // namespace YAudio

#endif /* YAUDIO_H */
