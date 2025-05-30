#include "yboard.h"

/////////////////////////////////// Global Yboard object ///////////////////////
YBoardV4 Yboard;

/////////////////////////////////// ISR Handling ///////////////////////////////
static TaskHandle_t isr_task_handle;
volatile bool mcp_isr_fired = false;

void IRAM_ATTR mcp_isr() {
    // mcp_isr_fired = true;
    xTaskNotifyGive(isr_task_handle);
}

void isr_task(void *pvParameters) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Serial.println("ISR fired");
        Yboard.recache_io_val_on_interrupt();
        Yboard.mcp.clearInterrupts();
    }
}
/////////////////////////////////// YBoarc Class Methods ///////////////////////

YBoardV4::YBoardV4()
    : display(128, 64, &upperWire), buttons_cached(0), sw_cached(0), dsw_cached(0),
      knob_button_cached(false) {
    FastLED.addLeds<APA102, led_data_pin, led_clock_pin, BGR>(leds, led_count);
}

YBoardV4::~YBoardV4() {}

void YBoardV4::setup() {
    // Setup interrupt handling task
    xTaskCreate(isr_task, "isr_task", 4096, NULL, 1, &isr_task_handle);

    setup_leds();
    setup_i2c();
    setup_io();

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

void YBoardV4::setup_i2c() {
    lowerWire.begin(this->lower_i2c_data, this->lower_i2c_clk, this->lower_i2c_freq);
    upperWire.begin(this->upper_i2c_data, this->upper_i2c_clk, this->upper_i2c_freq);
}

////////////////////////////// LEDs ///////////////////////////////
void YBoardV4::setup_leds() {
    FastLED.clear();
    set_led_brightness(120);
}

void YBoardV4::set_led_color(uint16_t index, uint8_t red, uint8_t green, uint8_t blue) {
    if (index < 1 || index >= led_count) {
        Serial.printf("ERROR: LED index %d out of range (1-%d)\n", index, led_count);
        return;
    }
    leds[index] = CRGB(red, green, blue);
    FastLED.show();
}

void YBoardV4::set_led_brightness(uint8_t brightness) {
    if (brightness > 220) {
        brightness = 220;
    }
    int max_brightness = 220;                           // Maximum brightness
    int total_steps = 220;                              // Number of fade steps
    float gamma = 2.2;                                  // Gamma value for correction
    float normalized = (float)brightness / total_steps; // Normalize step to [0, 1]
    int adjusted_brightness = max_brightness * pow(normalized, gamma);

    Serial.printf("Setting brightness to %d (%d)\n", adjusted_brightness, brightness);

    // Apply the brightness to all LEDs
    FastLED.setBrightness(adjusted_brightness);
    FastLED.show();
}

void YBoardV4::set_all_leds_color(uint8_t red, uint8_t green, uint8_t blue) {
    fill_solid(leds + 1, led_count - 1, CRGB(red, green, blue));
    FastLED.show();
}

////////////////////////////////// IO //////////////////////////////////
void YBoardV4::setup_io() {
    mcp.begin_I2C(gpio_addr, &lowerWire);

    // Setup pins for buttons/switches/dip switches
    mcp.pinMode(gpio_dsw1, INPUT);
    mcp.pinMode(gpio_dsw2, INPUT);
    mcp.pinMode(gpio_dsw3, INPUT);
    mcp.pinMode(gpio_dsw4, INPUT);
    mcp.pinMode(gpio_dsw5, INPUT);
    mcp.pinMode(gpio_dsw6, INPUT);
    mcp.pinMode(gpio_knob_but6, INPUT);
    mcp.pinMode(gpio_but5, INPUT);
    mcp.pinMode(gpio_but4, INPUT);
    mcp.pinMode(gpio_but3, INPUT);
    mcp.pinMode(gpio_but2, INPUT);
    mcp.pinMode(gpio_but1, INPUT);
    mcp.pinMode(gpio_sw1, INPUT);
    mcp.pinMode(gpio_sw2, INPUT);
    mcp.pinMode(gpio_sw3, INPUT);
    mcp.pinMode(gpio_sw4, INPUT);

    mcp.setupInterruptPin(gpio_dsw1, CHANGE);
    mcp.setupInterruptPin(gpio_dsw2, CHANGE);
    mcp.setupInterruptPin(gpio_dsw3, CHANGE);
    mcp.setupInterruptPin(gpio_dsw4, CHANGE);
    mcp.setupInterruptPin(gpio_dsw5, CHANGE);
    mcp.setupInterruptPin(gpio_dsw6, CHANGE);
    mcp.setupInterruptPin(gpio_knob_but6, CHANGE);
    mcp.setupInterruptPin(gpio_but5, CHANGE);
    mcp.setupInterruptPin(gpio_but4, CHANGE);
    mcp.setupInterruptPin(gpio_but3, CHANGE);
    mcp.setupInterruptPin(gpio_but2, CHANGE);
    mcp.setupInterruptPin(gpio_but1, CHANGE);
    mcp.setupInterruptPin(gpio_sw1, CHANGE);
    mcp.setupInterruptPin(gpio_sw2, CHANGE);
    mcp.setupInterruptPin(gpio_sw3, CHANGE);
    mcp.setupInterruptPin(gpio_sw4, CHANGE);

    // Setup interrupt for MCP
    // mirror INTA/B so only one wire required
    // active drive so INTA/B will not be floating
    // INTA/B will be signaled with a LOW
    mcp.setupInterrupts(true, false, LOW);

    // Set up interrupt pin coming from MCP
    pinMode(mcp_int_pin, INPUT);

    // Register ISR for MCP interrupt
    attachInterrupt(digitalPinToInterrupt(mcp_int_pin), mcp_isr, FALLING);

    // Clear any pending interrupts
    mcp.clearInterrupts();

    // Recache all IO values
    recache_all_io_vals();

    // Set up pins for rotary encoder
    ESP32Encoder::useInternalWeakPullResistors = puType::none;
    encoder.attachHalfQuad(rot_enc_b, rot_enc_a);
    encoder.clearCount();
}

////////////////////////////// Switches/Buttons ///////////////////////////////
bool YBoardV4::get_switch(uint8_t switch_idx) {
    if (switch_idx < 1 || switch_idx > 4) {
        return false;
    }

    return sw_cached & (1 << (switch_idx - 1));
}

uint8_t YBoardV4::get_switches() { return sw_cached; }

bool YBoardV4::get_button(uint8_t button_idx) {
    if (button_idx < 1 || button_idx > 5) {
        return false;
    }
    return buttons_cached & (1 << (button_idx - 1));
}

uint8_t YBoardV4::get_buttons() { return buttons_cached; }

int64_t YBoardV4::get_knob() { return encoder.getCount(); }

void YBoardV4::reset_knob() { encoder.clearCount(); }

void YBoardV4::set_knob(int64_t value) { encoder.setCount(value); }

bool YBoardV4::get_knob_button() { return knob_button_cached; }

bool YBoardV4::get_dip_switch(uint8_t dip_switch_idx) {
    if (dip_switch_idx < 1 || dip_switch_idx > 6) {
        return false;
    }
    return dsw_cached & (1 << (dip_switch_idx - 1));
}

uint8_t YBoardV4::get_dip_switches() { return dsw_cached; }

void YBoardV4::recache_all_io_vals() {
    buttons_cached = 0;
    for (int i = 0; i < 5; i++) {
        buttons_cached |= (!mcp.digitalRead(gpio_but1 + i)) << i;
    }

    dsw_cached = 0;
    for (int i = 0; i < 6; i++) {
        dsw_cached |= (!mcp.digitalRead(gpio_dsw1 + i)) << i;
    }

    sw_cached = 0;
    for (int i = 0; i < 4; i++) {
        sw_cached |= (mcp.digitalRead(gpio_sw1 + i)) << i;
    }

    knob_button_cached = !mcp.digitalRead(gpio_knob_but6);
}

void YBoardV4::recache_io_val_on_interrupt() {
    uint8_t interrupt_pin = mcp.getLastInterruptPin();
    if (interrupt_pin <= gpio_dsw6) {
        uint8_t dip_switch_idx = interrupt_pin - gpio_dsw1;
        bool dip_switch_state = !mcp.digitalRead(interrupt_pin);
        dsw_cached = (dsw_cached & ~(1 << dip_switch_idx)) | (dip_switch_state << dip_switch_idx);
    } else if (interrupt_pin == gpio_knob_but6) {
        knob_button_cached = !mcp.digitalRead(gpio_knob_but6);
    } else if (interrupt_pin <= gpio_but5) {
        uint8_t button_idx = interrupt_pin - gpio_but1;
        bool button_state = !mcp.digitalRead(interrupt_pin);
        buttons_cached = (buttons_cached & ~(1 << button_idx)) | (button_state << button_idx);
    } else if (interrupt_pin <= gpio_sw4) {
        uint8_t switch_idx = interrupt_pin - gpio_sw1;
        bool switch_state = mcp.digitalRead(interrupt_pin);
        sw_cached = (sw_cached & ~(1 << switch_idx)) | (switch_state << switch_idx);
    }
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
    if (!accel.begin(accel_addr, upperWire)) {
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
    if (!display.begin(SSD1306_SWITCHCAPVCC, display_addr)) {
        Serial.println("Error initializing display");
        return false;
    }

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setRotation(2);
    display.setTextWrap(false);
    display.setCursor(0, 0);
    display.display();

    return true;
}
