#include "yboard.h"

YBoardV4 Yboard;

YBoardV4::YBoardV4() : display(128, 32) {
    FastLED.addLeds<APA102, led_data_pin, led_clock_pin, BGR>(leds, led_count);
}

YBoardV4::~YBoardV4() {}

void YBoardV4::setup() {
    setup_leds();
    setup_switches();
    setup_buttons();

    if (setup_sd_card()) {
        Serial.println("SD Card Setup: Success");
    }

    if (setup_speaker()) {
        Serial.println("Speaker Setup: Success");
    }

    if (setup_mic()) {
        Serial.println("Mic Setup: Success");
    }

    if (setup_accelerometer()) {
        Serial.println("Accelerometer Setup: Success");
    }

    if (setup_display()) {
        Serial.println("Display Setup: Success");
    }
}

////////////////////////////// LEDs ///////////////////////////////
void YBoardV4::setup_leds() {
    FastLED.clear();
    set_led_brightness(50);
}

void YBoardV4::set_led_color(uint16_t index, uint8_t red, uint8_t green, uint8_t blue) {
    leds[index - 1] = CRGB(red, green, blue);
    FastLED.show();
}

void YBoardV4::set_led_brightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void YBoardV4::set_all_leds_color(uint8_t red, uint8_t green, uint8_t blue) {
    fill_solid(leds, led_count, CRGB(red, green, blue));
    FastLED.show();
}

////////////////////////////// Switches ///////////////////////////////
void YBoardV4::setup_switches() {
    pinMode(this->switch1_pin, INPUT);
    pinMode(this->switch2_pin, INPUT);
}

bool YBoardV4::get_switch(uint8_t switch_idx) {
    switch (switch_idx) {
    case 1:
        return digitalRead(this->switch1_pin);
    case 2:
        return digitalRead(this->switch2_pin);
    default:
        return false;
    }
}

////////////////////////////// Buttons ///////////////////////////////
void YBoardV4::setup_buttons() {
    pinMode(this->button1_pin, INPUT);
    pinMode(this->button2_pin, INPUT);
}

bool YBoardV4::get_button(uint8_t button_idx) {
    switch (button_idx) {
    case 1:
        return !digitalRead(this->button1_pin);
    case 2:
        return !digitalRead(this->button2_pin);
    default:
        return false;
    }
}

////////////////////////////// Knob ///////////////////////////////
int YBoardV4::get_knob() {
    int value = map(analogRead(this->knob_pin), 2888, 8, 0, 100);
    value = max(0, value);
    value = min(100, value);
    return value;
}

////////////////////////////// Speaker/Tones //////////////////////////////////
bool YBoardV4::setup_speaker() {

    if (!YAudio::setup_speaker(speaker_i2s_ws_pin, speaker_i2s_bclk_pin, speaker_i2s_data_pin,
                               speaker_i2s_port)) {
        Serial.println("ERROR: Speaker setup failed.");
        return false;
    }

    return true;
}

bool YBoardV4::play_sound_file(const std::string &filename) {
    if (!play_sound_file_background(filename)) {
        return false;
    }

    while (is_audio_playing()) {
        delay(10);
    }

    return true;
}

bool YBoardV4::play_sound_file_background(const std::string &filename) {
    // Prepend filename with a / if it doesn't have one
    std::string _filename = filename;
    if (_filename[0] != '/') {
        _filename.insert(0, "/");
    }

    if (!sd_card_present) {
        Serial.println("ERROR: SD Card not present.");
        return false;
    }

    if (!SD.exists(_filename.c_str())) {
        Serial.println("File does not exist.");
        return false;
    }

    return YAudio::play_sound_file(_filename);
}

void YBoardV4::set_sound_file_volume(uint8_t volume) { YAudio::set_wave_volume(volume); }

bool YBoardV4::play_notes(const std::string &notes) {
    if (!play_notes_background(notes)) {
        return false;
    }

    while (is_audio_playing()) {
        delay(10);
    }

    return true;
}

bool YBoardV4::play_notes_background(const std::string &notes) { return YAudio::add_notes(notes); }

void YBoardV4::stop_audio() { YAudio::stop_speaker(); }

bool YBoardV4::is_audio_playing() { return YAudio::is_playing(); }

I2SStream &YBoardV4::get_speaker_stream() { return YAudio::get_speaker_stream(); }

////////////////////////////// Microphone ////////////////////////////////////////
bool YBoardV4::setup_mic() {
    if (!YAudio::setup_mic(mic_i2s_ws_pin, mic_i2s_data_pin, mic_i2s_port)) {
        Serial.println("ERROR: Mic setup failed.");
        return false;
    }

    YAudio::set_recording_gain(10);

    return true;
}

bool YBoardV4::start_recording(const std::string &filename) {
    // Prepend filename with a / if it doesn't have one
    std::string _filename = filename;
    if (_filename[0] != '/') {
        _filename.insert(0, "/");
    }

    if (!sd_card_present) {
        Serial.println("ERROR: SD Card not present.");
        return false;
    }

    return YAudio::start_recording(_filename);
}

void YBoardV4::stop_recording() { YAudio::stop_recording(); }

bool YBoardV4::is_recording() { return YAudio::is_recording(); }

void YBoardV4::set_recording_volume(uint8_t volume) { YAudio::set_recording_gain(volume); }

I2SStream &YBoardV4::get_microphone_stream() { return YAudio::get_mic_stream(); }

////////////////////////////// Accelerometer /////////////////////////////////////
bool YBoardV4::setup_accelerometer() {
    if (!wire_begin) {
        Wire.begin(sda_pin, scl_pin);
        wire_begin = true;
    }

    if (!accel.begin(accel_addr, Wire)) {
        Serial.println("WARNING: Accelerometer not detected.");
        return false;
    }

    return true;
}

bool YBoardV4::accelerometer_available() { return accel.available(); }

accelerometer_data YBoardV4::get_accelerometer() {
    accelerometer_data data;
    data.x = accel.getX();
    data.y = accel.getY();
    data.z = accel.getZ();
    return data;
}

bool YBoardV4::setup_sd_card() {
    // Set microSD Card CS as OUTPUT and set HIGH
    pinMode(sd_cs_pin, OUTPUT);
    digitalWrite(sd_cs_pin, HIGH);

    // Initialize SPI bus for microSD Card
    SPI.begin(spi_sck_pin, spi_miso_pin, spi_mosi_pin);

    // Start microSD Card
    if (!SD.begin(sd_cs_pin)) {
        Serial.println("Error accessing microSD card!");
        sd_card_present = false;
        return false;
    }

    sd_card_present = true;

    return true;
}

bool YBoardV4::setup_display() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {
        Serial.println("Error initializing display");
        return false;
    }

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setRotation(0);
    display.setTextWrap(false);
    display.setCursor(0, 0);
    display.display();

    return true;
}
