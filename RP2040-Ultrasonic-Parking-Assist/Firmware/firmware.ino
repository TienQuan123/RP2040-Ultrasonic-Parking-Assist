#include <Arduino.h>
#include "pico/time.h"

// ===================================================================
// == PHẦN 1A: THƯ VIỆN PicoSoftUART (CHO CẢM BIẾN) ==================
// ===================================================================
#define SU_RX_BUFFER_SIZE 64
#define SU_TX_BUFFER_SIZE 32

class PicoSoftUART {
public:
    PicoSoftUART(uint8_t txPin, uint8_t rxPin);
    void begin(long speed);
    int read();
    size_t write(uint8_t byte);
    int available();
    void print(String s);
    void println(String s);
    String readStringUntil(char terminator);
    void handleTimerInterrupt();

private:
    void txProcess();
    void rxProcess();
    uint8_t _txPin, _rxPin;
    long _bitPeriod;
    uint8_t _rx_buffer[SU_RX_BUFFER_SIZE];
    volatile uint8_t _rx_buffer_head, _rx_buffer_tail;
    uint8_t _tx_buffer[SU_TX_BUFFER_SIZE];
    volatile uint8_t _tx_buffer_head, _tx_buffer_tail;
    volatile uint8_t _tx_bit_counter, _tx_byte, _rx_bit_counter, _rx_byte, _sample_counter, _rx_bit_offset;
    volatile bool _rx_timing_flag;
};

static PicoSoftUART* p_suart_instance = nullptr;

bool repeating_timer_callback(struct repeating_timer *t) {
    if (p_suart_instance) {
        p_suart_instance->handleTimerInterrupt();
    }
    return true;
}

PicoSoftUART::PicoSoftUART(uint8_t txPin, uint8_t rxPin) {
    _txPin = txPin;
    _rxPin = rxPin;
    p_suart_instance = this;
}

void PicoSoftUART::begin(long speed) {
    _bitPeriod = 1000000 / speed;
    pinMode(_txPin, OUTPUT);
    digitalWrite(_txPin, HIGH);
    pinMode(_rxPin, INPUT_PULLUP);
    _rx_buffer_head = 0;
    _rx_buffer_tail = 0;
    _tx_buffer_head = 0;
    _tx_buffer_tail = 0;
    _tx_bit_counter = 0;
    _rx_bit_counter = 0;
    _sample_counter = 0;
    _rx_timing_flag = false;
    long timer_interval = -(_bitPeriod / 5);
    struct repeating_timer *timer = new struct repeating_timer;
    add_repeating_timer_us(timer_interval, repeating_timer_callback, NULL, timer);
}

void PicoSoftUART::handleTimerInterrupt() {
    rxProcess();
    if (_sample_counter == 0) {
        txProcess();
    }
}

void PicoSoftUART::txProcess() {
    if (_tx_bit_counter > 0) {
        _tx_bit_counter--;
        if (_tx_bit_counter == 10) {
            digitalWrite(_txPin, LOW);
        } else if (_tx_bit_counter > 1) {
            digitalWrite(_txPin, (_tx_byte >> (9 - _tx_bit_counter)) & 1);
        } else {
            digitalWrite(_txPin, HIGH);
        }
    } else if (_tx_buffer_head != _tx_buffer_tail) {
        _tx_byte = _tx_buffer[_tx_buffer_tail];
        _tx_buffer_tail = (_tx_buffer_tail + 1) % SU_TX_BUFFER_SIZE;
        _tx_bit_counter = 11;
    }
}

void PicoSoftUART::rxProcess() {
    bool pin_state = digitalRead(_rxPin);
    
    if (!_rx_timing_flag && !pin_state) {
        _rx_bit_offset = (_sample_counter + 2) % 5;
        _rx_timing_flag = true;
        _rx_bit_counter = 10;
        _rx_byte = 0;
    }

    if (_rx_timing_flag && (_sample_counter == _rx_bit_offset)) {
        if (_rx_bit_counter > 0) {
            _rx_bit_counter--;
            if (_rx_bit_counter > 0) {
                _rx_byte >>= 1;
                if (pin_state) {
                    _rx_byte |= 0x80;
                }
            } else {
                if (pin_state) {
                    uint8_t next_head = (_rx_buffer_head + 1) % SU_RX_BUFFER_SIZE;
                    if (next_head != _rx_buffer_tail) {
                        _rx_buffer[_rx_buffer_head] = _rx_byte;
                        _rx_buffer_head = next_head;
                    }
                }
                _rx_timing_flag = false;
            }
        }
    }
    _sample_counter = (_sample_counter + 1) % 5;
}

int PicoSoftUART::available() {
    return (_rx_buffer_head + SU_RX_BUFFER_SIZE - _rx_buffer_tail) % SU_RX_BUFFER_SIZE;
}

int PicoSoftUART::read() {
    if (_rx_buffer_head == _rx_buffer_tail) {
        return -1;
    }
    uint8_t c = _rx_buffer[_rx_buffer_tail];
    _rx_buffer_tail = (_rx_buffer_tail + 1) % SU_RX_BUFFER_SIZE;
    return c;
}

size_t PicoSoftUART::write(uint8_t byte) {
    uint8_t next_head = (_tx_buffer_head + 1) % SU_TX_BUFFER_SIZE;
    if (next_head == _tx_buffer_tail) {
        return 0;
    }
    _tx_buffer[_tx_buffer_head] = byte;
    _tx_buffer_head = next_head;
    return 1;
}

void PicoSoftUART::print(String s) {
    for (int i = 0; i < s.length(); i++) {
        write(s[i]);
    }
}

void PicoSoftUART::println(String s) {
    print(s);
    write('\n');
}

String PicoSoftUART::readStringUntil(char terminator) {
    String ret;
    int c = read();
    while (c >= 0 && c != terminator) {
        ret += (char)c;
        c = read();
    }
    return ret;
}

// ===================================================================
// == PHẦN 1B: THƯ VIỆN SoftUART_UI (CHO GIAO DIỆN) ================
// ===================================================================
class SoftUART_UI {
private:
    uint8_t _txPin, _rxPin;
    long _bitPeriod;

public:
    SoftUART_UI(uint8_t txPin, uint8_t rxPin) {
        _txPin = txPin;
        _rxPin = rxPin;
    }
    
    void begin(long speed) {
        _bitPeriod = 1000000 / speed;
        pinMode(_txPin, OUTPUT);
        digitalWrite(_txPin, HIGH);
        pinMode(_rxPin, INPUT_PULLUP);
    }
    
    size_t write(uint8_t byte) {
        digitalWrite(_txPin, LOW);
        delayMicroseconds(_bitPeriod);
        for (int i = 0; i < 8; i++) {
            if (byte & 1) {
                digitalWrite(_txPin, HIGH);
            } else {
                digitalWrite(_txPin, LOW);
            }
            byte >>= 1;
            delayMicroseconds(_bitPeriod);
        }
        digitalWrite(_txPin, HIGH);
        delayMicroseconds(_bitPeriod);
        return 1;
    }
    
    bool available() {
        return (digitalRead(_rxPin) == LOW);
    }
    
    int read() {
        while (digitalRead(_rxPin) == HIGH) {
            yield();
        }
        delayMicroseconds(_bitPeriod * 1.5);
        uint8_t data = 0;
        for (int i = 0; i < 8; i++) {
            if (digitalRead(_rxPin) == HIGH) {
                data |= (1 << i);
            }
            delayMicroseconds(_bitPeriod);
        }
        return data;
    }
    
    void print(String s) {
        for (int i = 0; i < s.length(); i++) {
            write(s[i]);
        }
    }
    
    void println(String s) {
        print(s);
        write('\n');
    }
    
    String readStringUntil(char terminator) {
        String ret;
        char c;
        while (1) {
            if (available()) {
                c = (char)read();
                if (c == terminator) {
                    break;
                }
                ret += c;
            } else {
                yield();
            }
        }
        return ret;
    }
};

// ===================================================================
// == PHẦN 2: CÀI ĐẶT VÀ KHAI BÁO BIẾN TOÀN CỤC =====================
// ===================================================================
#define LED_PIN 25 

PicoSoftUART sensorUART(0, 1);
SoftUART_UI uiUART(12, 13);

enum Mode {
    IDLE,
    READ,
    TEST
};
Mode currentMode = IDLE;

#define NUM_SAMPLES 10
float samples[NUM_SAMPLES];
int sampleCounter = 0;
unsigned long lastSampleTime = 0;

enum ReadState {
    READ_IDLE,
    READ_WAITING_FOR_SENSOR
};
ReadState currentReadState = READ_IDLE;
unsigned long readStateTimer = 0;

// ===================================================================
// == PHẦN 3: HÀM HỖ TRỢ (CRC & TẠO FRAME) =========================
// ===================================================================
uint8_t crc(const char *s) {
    uint16_t sum = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        sum += (uint8_t)s[i];
    }
    return (uint8_t)(sum % 256);
}

String make_frame(float val) {
    char core_buffer[32];
    snprintf(core_buffer, 32, "@DIST_%.1f_#", val);
    String frame = String(core_buffer);
    frame += String(crc(core_buffer));
    return frame;
}

// ===================================================================
// == PHẦN 4: CHƯƠNG TRÌNH CHÍNH (ĐÃ CẬP NHẬT LOGIC) ================
// ===================================================================
void setup() {
    Serial.begin(115220);
    pinMode(LED_PIN, OUTPUT); 
    digitalWrite(LED_PIN, LOW); 
    delay(2000);
    
    Serial.println("--- Firmware da san sang (Phien ban 3 - LED & CRC) ---");
    
    uiUART.begin(9600);
    Serial.println("UART Giao dien (Software) da khoi tao tren chan 12, 13.");
    
    sensorUART.begin(9600);
    Serial.println("UART Cam bien (Software) da khoi tao tren chan 0, 1.");
    
    uiUART.println("FIRMWARE_READY");
}

void run_read_mode_non_blocking() {
    switch (currentReadState) {
        case READ_IDLE:
            if (millis() - lastSampleTime >= 500) {
                lastSampleTime = millis();
                sensorUART.write(0x55);
                currentReadState = READ_WAITING_FOR_SENSOR;
                readStateTimer = millis();
            }
            break;

        case READ_WAITING_FOR_SENSOR:
            if (millis() - readStateTimer < 100) {
                return; 
            }

            bool validSample = false;
            float distance = 0.0;

            if (sensorUART.available() >= 2) {
                uint8_t high_byte = sensorUART.read();
                uint8_t low_byte = sensorUART.read();
                distance = ((high_byte << 8) | low_byte) / 10.0;
                
                if (distance > 2 && distance < 450) {
                    Serial.print("[READ_SM] Doc lan " + String(sampleCounter + 1) + ": " + String(distance, 1) + " cm\n");
                    validSample = true;
                }
            }


            while (sensorUART.available()) {
                sensorUART.read();
            }

            if (validSample) {
                samples[sampleCounter++] = distance;
                
                if (sampleCounter >= NUM_SAMPLES) {
                    float sum = 0.0;
                    for (int i = 0; i < NUM_SAMPLES; i++) {
                        sum += samples[i];
                    }
                    float avg = sum / NUM_SAMPLES;

                    
                    if (avg < 10.0) {
                        digitalWrite(LED_PIN, HIGH); 
                    } else {
                        digitalWrite(LED_PIN, LOW); 
                    }

                    String frame_to_send = make_frame(avg);
                    uiUART.println(frame_to_send);
                    Serial.println("[READ_SM] Da gui frame: " + frame_to_send);
                    sampleCounter = 0;
                }
            }
            
            currentReadState = READ_IDLE;
            break;
    }
}

void run_dedicated_test_mode() {
    Serial.println("Entering dedicated TEST mode loop (with CRC check)...");
    
    while (currentMode == TEST) {
        if (uiUART.available()) {
            String line = uiUART.readStringUntil('\n');
            line.trim();

            if (line.equalsIgnoreCase("STOP")) {
                currentMode = IDLE;
                Serial.println("Nhan lenh STOP -> Thoat che do TEST.");
                uiUART.println("ACK_IDLE_MODE");
                
            } else if (line.equalsIgnoreCase("READ")) {
                currentMode = READ;
                Serial.println("Nhan lenh READ -> Thoat che do TEST.");
                uiUART.println("ACK_READ_MODE");
                sampleCounter = 0;
                currentReadState = READ_IDLE;
                lastSampleTime = millis() - 500; 
                
            } else if (line.length() > 0) {
                
                int separatorIndex = line.lastIndexOf('#');
                
                if (separatorIndex != -1) {
                    String payload = line.substring(0, separatorIndex);
                    String crcStr = line.substring(separatorIndex + 1);
                    uint8_t receivedCrc = crcStr.toInt();
                    
                    // Tính toán CRC từ payload
                    uint8_t calculatedCrc = crc(payload.c_str());

                    // So sánh và chỉ echo nếu CRC hợp lệ
                    if (receivedCrc == calculatedCrc) {
                        uiUART.println(line); // Echo lại toàn bộ gói tin
                        Serial.println("[TEST_CRC_OK] Echo: " + line);
                    } else {
                        Serial.println("[TEST_CRC_FAIL] Goi tin bi loi, bo qua. Nhan: " + line);
                    }
                } else {
                    Serial.println("[TEST_NO_CRC] Goi tin khong co CRC, bo qua: " + line);
                }
            }
        }
        yield(); 
    }
    
    Serial.println("Exited dedicated TEST mode loop.");
}

void loop() {
    // 1. Kiểm tra lệnh từ UI
    if (uiUART.available()) {
        String command = uiUART.readStringUntil('\n');
        command.trim();

        if (command.equalsIgnoreCase("READ")) {
            currentMode = READ;
            sampleCounter = 0;
            currentReadState = READ_IDLE;
            lastSampleTime = millis() - 500; 
            Serial.println("Nhan lenh READ.");
            uiUART.println("ACK_READ_MODE");
            
        } else if (command.equalsIgnoreCase("TEST")) {
            currentMode = TEST;
            digitalWrite(LED_PIN, LOW); 
            Serial.println("Nhan lenh TEST.");
            uiUART.println("ACK_TEST_MODE");
            
        } else if (command.equalsIgnoreCase("STOP")) {
            currentMode = IDLE;
            digitalWrite(LED_PIN, LOW); 
            Serial.println("Nhan lenh STOP.");
            uiUART.println("ACK_IDLE_MODE");
        }
    }

    // 2. Chạy State Machine dựa trên chế độ hiện tại
    if (currentMode == READ) {
        run_read_mode_non_blocking();
    }
    else if (currentMode == TEST) {
        run_dedicated_test_mode(); // Hàm này chứa vòng lặp riêng
    }
    else { // currentMode == IDLE
        yield(); // Không làm gì cả, nghỉ
    }
}
