#ifndef YBOARDV4_H
#define YBOARDV4_H

#include <Adafruit_MCP23X17.h>
#include <Adafruit_SSD1306.h>
#include <AudioTools.h>
#include <ESP32Encoder.h>
#include <FS.h>
#include <FastLED.h>
#include <SD.h>
#include <SparkFun_LIS2DH12.h>
#include <stdint.h>

#include "yaudio.h"

struct accelerometer_data {
    float x;
    float y;
    float z;
};

class YBoardV4 {
  public:
    YBoardV4();
    virtual ~YBoardV4();

    /*
     *  This function initializes the YBoard. This function must be called before using any of the
     * YBoard features.
     */
    void setup();

    ////////////////////////////// LEDs ///////////////////////////////////////////

    /*
     *  This function sets the color of an individual LED.
     *  The index is an integer between 1 and 20, representing the number of the
     * target LED (for example, 1 corresponds to the first LED on the board).
     *  The red, green, and blue values are integers between 0 and 255, representing
     * the intensity of the corresponding color. For example, if you want to set the
     * LED to red, you would set red to 255 and green and blue to 0.
     */
    void set_led_color(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);

    /*
     *  This function sets the brightness of all the LEDs on the board.
     *  The brightness is an integer between 0 and 255, representing the intensity
     * of the LEDs. A brightness of 0 is off, and a brightness of 255 is full
     * brightness.
     */
    void set_led_brightness(uint8_t brightness);

    /*
     *  This function sets the color of all the LEDs on the board.
     *  The red, green, and blue values are integers between 0 and 255, representing
     * the intensity of the corresponding color. For example, if you want to set all
     * the LEDs to red, you would set red to 255 and green and blue to 0.
     */
    void set_all_leds_color(uint8_t red, uint8_t green, uint8_t blue);

    ////////////////////////////// Switches/Buttons ///////////////////////////////
    /*
     *  This function returns the state of a switch.
     *  The switch_idx is an integer between 1 and 4, representing the number of
     * the target switch (for example, 1 corresponds to switch 1 on the board).
     * Switches are numbered from left to right, 1 to 4. The bool return type
     * means that this function returns a boolean value (true or false).
     *  True corresponds to the switch being on, and false corresponds to the switch
     * being off.
     */
    bool get_switch(uint8_t switch_idx);

    /*
     *  This function returns the state of all switches on the board.
     *  Bit 0 corresponds to switch 1, bit 1 corresponds to switch 2, and so on.
     */
    uint8_t get_switches();

    /*
     *  This function returns the state of a button.
     *  The button_idx is an integer between 1 and 5, representing the number of the
     * target button (for example, 1 corresponds to button 1 on the board). The bool
     * return type means that this function returns a boolean value (true or false).
     *  True corresponds to the button being pressed, and false corresponds to the
     * button being released.
     */
    bool get_button(uint8_t button_idx);

    /*
     *  This function returns the state of all buttons on the board.
     *  Bit 0 corresponds to button 1, bit 1 corresponds to button 2, and so on.
     */
    uint8_t get_buttons();

    /*
     *  This function returns the value of the knob.
     */
    int64_t get_knob();

    bool get_knob_button();

    void reset_knob();

    void set_knob(int64_t value);

    /*
     *  This function returns the state of a DIP switch.
     *  The dip_switch_idx is an integer between 1 and 6, representing the number of
     * target DIP switch (for example, 1 corresponds to DIP switch 1 on the board).
     * The bool return type means that this function returns a boolean value (true or false).
     *  True corresponds to the DIP switch being on, and false corresponds to the DIP switch
     * being off.
     */
    bool get_dip_switch(uint8_t dip_switch_idx);

    /*
     *  This function returns the state of all DIP switches on the board.
     *  Bit 0 corresponds to DIP switch 1, bit 1 corresponds to DIP switch 2, and so on.
     */
    uint8_t get_dip_switches();

    ////////////////////////////// Speaker/Tones //////////////////////////////////
    /*
     *  This function plays a sound on the speaker. The filename is a string
     * representing the name of the sound file to play. The return type is a boolean
     * value (true or false). True corresponds to the sound being played
     * successfully, and false corresponds to an error playing the sound. The sound
     * file must be stored on the microSD card.
     */
    bool play_sound_file(const std::string &filename);

    /* This is similar to the function above, except that it will start the song playing
     * in the background and return immediately. The song will continue to play in the
     * background until it is stopped with the stop_audio function, another song is
     * played, play_notes is called, or the song finishes.
     */
    bool play_sound_file_background(const std::string &filename);

    /*
     * This function sets the speaker volume when playing a sound file. The volume
     * is an integer between 0 and 10. A volume of 0 is off, and a volume of 10 is full volume.
     * This does not affect the volume of notes played with the play_notes function, as
     * their volume is set with the V command in the notes themselves.
     */
    void set_sound_file_volume(uint8_t volume);

    /* Plays the specified sequence of notes. The function will return once the notes
     * have finished playing.
     *
     * A–G	                Specifies a note that will be played.
     * R                    Specifies a rest (no sound for the duration of the note).
     * + or # after a note  Raises the preceding note one half-step (sharp).
     * - after a note	      Lowers the preceding note one half-step.
     * > after a note	      Plays the note one octave higher (multiple >’s can be used, eg: C>>)
     * < after a note	      Plays the note one octave lower (multiple <’s can be used, eg: C<<)
     * 1–2000 after a note	Determines the duration of the preceding note. For example,
     *                      C16 specifies C played as a sixteenth note, B1 is B played as a whole
     *                      note. If no duration is specified, the note is played as a quarter note.
     * O followed by a #    Changes the octave. Valid range is 4-7. Default is 5.
     * T followed by a #    Changes the tempo. Valid range is 40-240. Default is 120.
     * V followed by a #    Changes the volume.  Valid range is 1-10. Default is 5.
     * !                    Resets octave, tempo, and volume to default values.
     * spaces               Spaces can be placed between notes or commands for readability,
     *                      but not within a note or command (eg: "C4# D4" is valid, "C 4 # D 4" is
     *                      not. "T120 A B C" is valid, "T 120 A B C" is not).
     */
    bool play_notes(const std::string &new_notes);

    /* This is similar to the function above, except that it will start playing the notes
     * in the background and return immediately. The notes will continue to play in the
     * background until they are stopped with the stop_audio function, a WAVE file is played,
     * or the notes finish. If you call this function again before the notes finish, the
     * the new notes will be appended to the end of the current notes.  This allows you to
     * call this function multiple times to build up multiple sequences of notes to play.
     */
    bool play_notes_background(const std::string &new_notes);

    /*
     * This function stops the audio from playing (either a song or a sequence of notes)
     */
    void stop_audio();

    /*
     *  This function returns whether audio is playing.
     */
    bool is_audio_playing();

    /*
     * This function returns the speaker stream object which can be used to take
     * control of the speaker, beyond playing a tone or a file, which this
     * library already provides. This is an advanced function. To see what you
     * can do with a speaker stream object, you can view
     * https://github.com/pschatzmann/arduino-audio-tools.
     */
    I2SStream &get_speaker_stream();

    ////////////////////////////// Microphone ////////////////////////////////////////
    /*
     *  This function starts recording audio from the microphone. The filename is a
     * string representing the name of the file to save the recording to. The return
     * type is a boolean value (true or false). True corresponds to the recording
     * starting successfully, and false corresponds to an error starting the recording.
     * The recording will continue until stop_recording is called.
     */
    bool start_recording(const std::string &filename);

    /*
     *  This function stops recording audio from the microphone.
     */
    void stop_recording();

    /*
     *  This function returns whether the microphone is currently recording.
     */
    bool is_recording();

    /*
     *  This function sets the volume of the microphone when recording. The volume is
     * an integer between 0 and 12. A volume of 0 is off, and a volume of 12 is full volume.
     */
    void set_recording_volume(uint8_t volume);

    /*
     * This function returns the microphone stream object which can be used to take
     * control of the microphone, beyond recording to a file, which this
     * library already provides. This is an advanced function. To see what you
     * can do with a microphone stream object, you can view
     * https://github.com/pschatzmann/arduino-audio-tools.
     */
    I2SStream &get_microphone_stream();

    ///////////////////////////// Accelerometer ////////////////////////////////////
    /*
     *  This function returns whether accelerometer data is available.
     *  The bool return type means that this function returns a boolean value (true
     * or false). True corresponds to accelerometer data being available, and false
     * corresponds to accelerometer data not being available.
     */
    bool accelerometer_available();

    /*
     *  This function returns the accelerometer data.
     *  The return type is a struct containing the x, y, and z values of the
     * accelerometer data. These values are floats, representing the acceleration
     * in the x, y, and z directions, respectively.
     */
    accelerometer_data get_accelerometer();

    /*
     *  This function fetches the I/O values from the GPIO multiplexer,
     *  and stores them in the cached array.
     */
    void recache_all_io_vals();

    /*
     *  This function recaches just a single I/O value from the GPIO multiplexer,
     *  based on the last interrupt that was triggered.
     */
    void recache_io_val_on_interrupt();

    // Display
    Adafruit_SSD1306 display;

    // Rotary Encoder
    ESP32Encoder encoder;

    // GPIO multiplixer
    Adafruit_MCP23X17 mcp;

    // Button indices
    static constexpr int button_left = 1;
    static constexpr int button_right = 2;
    static constexpr int button_up = 3;
    static constexpr int button_down = 4;
    static constexpr int button_center = 5;

    // LEDs
    static constexpr int led_count = 36;

  private:
    bool wire_begin = false;
    bool sd_card_present = false;

    // Buttons are stored in a bitmask, with button1 at bit 0, button2 at bit 1, etc.
    // This is cached from the GPIO multiplexer.
    uint8_t buttons_cached;

    // Switches, same format as buttons
    uint8_t sw_cached;

    // Dip switches, same format as buttons
    uint8_t dsw_cached;

    bool knob_button_cached;

    // LEDs
    CRGB leds[led_count];

    // I2C buses
    TwoWire upperWire = TwoWire(0);
    TwoWire lowerWire = TwoWire(1);

    // Accelerometer
    SPARKFUN_LIS2DH12 accel;

    // LEDs
    static constexpr int led_clock_pin = 4;
    static constexpr int led_data_pin = 5;

    // GPIO Multiplexer
    static constexpr int gpio_dsw1 = 0;
    static constexpr int gpio_dsw2 = 1;
    static constexpr int gpio_dsw3 = 2;
    static constexpr int gpio_dsw4 = 3;
    static constexpr int gpio_dsw5 = 4;
    static constexpr int gpio_dsw6 = 5;
    static constexpr int gpio_knob_but6 = 6;
    static constexpr int gpio_but1 = 7;
    static constexpr int gpio_but2 = 8;
    static constexpr int gpio_but3 = 9;
    static constexpr int gpio_but4 = 10;
    static constexpr int gpio_but5 = 11;
    static constexpr int gpio_sw1 = 12;
    static constexpr int gpio_sw2 = 13;
    static constexpr int gpio_sw3 = 14;
    static constexpr int gpio_sw4 = 15;
    static constexpr int mcp_int_pin = 16;

    // Rotary Encoder
    static constexpr int rot_enc_a = 37;
    static constexpr int rot_enc_b = 38;

    // I2C Connections
    static constexpr int sda_pin = 2;
    static constexpr int scl_pin = 1;
    static constexpr int upper_i2c_freq = 100000;
    static constexpr int upper_i2c_data = 2;
    static constexpr int upper_i2c_clk = 1;
    static constexpr int lower_i2c_freq = 100000;
    static constexpr int lower_i2c_data = 18;
    static constexpr int lower_i2c_clk = 17;

    // I2C Devices
    static constexpr int accel_addr = 0x19;
    static constexpr int display_addr = 0x3c;
    static constexpr int gpio_addr = 0x20;

    // microSD Card Reader connections
    static constexpr int sd_cs_pin = 10;
    static constexpr int spi_mosi_pin = 11;
    static constexpr int spi_miso_pin = 13;
    static constexpr int spi_sck_pin = 12;

    // I2S Speaker Connections
    static constexpr int speaker_i2s_data_pin = 14;
    static constexpr int speaker_i2s_bclk_pin = 21;
    static constexpr int speaker_i2s_ws_pin = 47;
    static constexpr int speaker_i2s_port = 1;

    // I2S Microphone Connections
    static constexpr int mic_i2s_ws_pin = 41;
    static constexpr int mic_i2s_data_pin = 40;
    static constexpr int mic_i2s_port = 0;

    void setup_i2c();
    void setup_leds();
    void setup_io();
    bool setup_speaker();
    bool setup_mic();
    bool setup_accelerometer();
    bool setup_sd_card();
    bool setup_display();
};

extern YBoardV4 Yboard;

#endif /* YBOARDV4_H */
