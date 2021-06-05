//
// 1KB Racing game for Attiny13A (1024 bytes flash / 64 bytes ram) with oled display 128x64 (ssd1306)
// Author: Alexey Kaspin
//
// compilation flags: -mno-interrupts -DNDEBUG -Os -ffunction-sections -fdata-sections
//                    -fpack-struct -fshort-enums -mmcu=attiny13a -std=c++11 -fno-caller-saves -mtiny-stack -ffreestanding"
//
// link flags: -nostartfiles --gc-sections -mmcu=attiny13a -mrelax
//

// default attiny13a fuses = 1MHz
#define F_CPU 1000000L

#include <avr/io.h>
#include <util/delay.h>

// 4-wire connection to oled1306 display
//
#define D_DIN PORTB3 // D1
#define D_CLK PORTB4 // D0
#define D_DC  PORTB2
#define D_RES PORTB1

#define OLED1306_PORT_HI(p) (PORTB |= (1 << (p)))
#define OLED1306_PORT_LO(p) (PORTB &= ~(1 << (p)))

template <uint8_t... Chars> struct chars {
private:
    template <void (*f)(uint8_t), typename = void> inline static void _apply() {}
    template <void (*f)(uint8_t), uint8_t Ch, uint8_t... Chs> inline static void _apply() {
        f(Ch);
        _apply<f, Chs...>();
    }

    template <void (*f)(uint8_t), typename = void> inline static void _apply_inv() {}
    template <void (*f)(uint8_t), uint8_t Ch, uint8_t... Chs> inline static void _apply_inv() {
        _apply_inv<f, Chs...>();
        f(Ch);
    }
        
public:
    template <void (*f)(uint8_t)> static void apply() {
        _apply<f, Chars...>();
    }
        
    template <void (*f)(uint8_t)> static void apply_inv() {
        _apply_inv<f, Chars...>();
    }
};

static void send_value(uint8_t value) {
    OLED1306_PORT_LO(D_CLK);
            
    for (uint8_t i = 0; i < 8; i++) {
        OLED1306_PORT_LO(D_DIN);
                
        if (value & 0b10000000) {
            OLED1306_PORT_HI(D_DIN);
        }
                
        value <<= 1;
                
        OLED1306_PORT_HI(D_CLK);
        OLED1306_PORT_LO(D_CLK);
    }
}
    
static void __attribute__ ((noinline)) send_command(uint8_t cmd) {
    OLED1306_PORT_LO(D_DC);
    send_value(cmd);
}

static void __attribute__ ((noinline)) send_data(uint8_t dat) {
    OLED1306_PORT_HI(D_DC);
    send_value(dat);
}

static void set_page(uint8_t page) {
    send_command(0x22);
    send_command(page);
    send_command(7);
}

static void set_coord_range(uint8_t start, uint8_t end) {
    send_command(0x21);
    send_command(start);
    send_command(end);
}

static void set_coord(uint8_t coord) {
    send_command(0x21);
    send_command(coord);
    send_command(127);
}

static void clear() {
    set_page(0);
    set_coord(0);
    
    uint16_t i = 8 * 128;
    
    do {
        send_data(0x00);
    }
    while (--i);
}

static void init() {
    OLED1306_PORT_HI(D_CLK);
    OLED1306_PORT_LO(D_RES);
    
    volatile uint8_t i = 255;
    do {
        asm volatile("nop");
    }
    while(--i);
    
    OLED1306_PORT_HI(D_RES);
    
    chars<0xAE, 0xD5, 0x80, 0xA8, 0x3f, 0x20, 0x00, 0xAF>::apply<send_command>();
}

static const uint8_t CAR_CENTER = 58;
static const uint8_t CAR_MAX_OFFSET = 12;
static const uint8_t CAR_START_OFFSET = CAR_CENTER - CAR_MAX_OFFSET;
static const uint8_t CAR_LEFT_SIDE = 54;
static const uint8_t CAR_RIGHT_SIDE = 62;
static const uint8_t BARRIER_MIN_INDEX = 32;
static const uint8_t BARRIER_DANGER_INDEX = 152;
static const uint8_t BARRIER_STEP_1 = 64;
static const uint8_t BARRIER_STEP_2 = 96;
static const uint8_t BARRIER_STEP_4 = 176;
static const uint8_t START_FRAME_TIME = 30;

using car_side = chars<
    // 12x8 px image
    0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
    0b01110000,  // 0x70  ▓░░░▓▓▓▓
    0b11101100,  // 0xEC  ░░░▓░░▓▓
    0b11100110,  // 0xE6  ░░░▓▓░░▓
    0b10100111,  // 0xA7  ░▓░▓▓░░░
    0b00100111,  // 0x27  ▓▓░▓▓░░░
    0b10100111,  // 0xA7  ░▓░▓▓░░░
    0b11100111,  // 0xE7  ░░░▓▓░░░
    0b11100111,  // 0xE7  ░░░▓▓░░░
    0b01111111,  // 0x7F  ▓░░░░░░░
    0b01100110,  // 0x66  ▓░░▓▓░░▓
    0b00000000   // 0x00  ▓▓▓▓▓▓▓▓
>;

using car_center = chars<
    // 12x8 px image
    0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
    0b01111000,  // 0x78  ▓░░░░▓▓▓
    0b11101110,  // 0xEE  ░░░▓░░░▓
    0b11100111,  // 0xE7  ░░░▓▓░░░
    0b10100111,  // 0xA7  ░▓░▓▓░░░
    0b00100111,  // 0x27  ▓▓░▓▓░░░
    0b10100111,  // 0xA7  ░▓░▓▓░░░
    0b11100111,  // 0xE7  ░░░▓▓░░░
    0b11101110,  // 0xEE  ░░░▓░░░▓
    0b01111000,  // 0x78  ▓░░░░▓▓▓
    0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
    0b00000000   // 0x00  ▓▓▓▓▓▓▓▓
>;

using car_over = chars<
    // 12x8 px image
    0b00010100,  // 0x14  ▓▓▓░▓░▓▓
    0b11011001,  // 0xD9  ░░▓░░▓▓░
    0b11011110,  // 0xDE  ░░▓░░░░▓
    0b00100110,  // 0x26  ▓▓░▓▓░░▓
    0b11111010,  // 0xFA  ░░░░░▓░▓
    0b10110011,  // 0xB3  ░▓░░▓▓░░
    0b00100111,  // 0x27  ▓▓░▓▓░░░
    0b11110101,  // 0xF5  ░░░░▓░▓░
    0b01111110,  // 0x7E  ▓░░░░░░▓
    0b11011100,  // 0xDC  ░░▓░░░▓▓
    0b11001010,  // 0xCA  ░░▓▓░▓░▓
    0b00010000   // 0x10  ▓▓▓░▓▓▓▓
>;

static uint16_t roadline[4] = {
    0b0011001101001010,
    0b0110011001010101,
    0b1100110010101010,
    0b1001100110110101,
};

static uint8_t numbers[20] = {
    // #048 Number '0' (U+0030 Digit Zero)
    0x7C,  // ▓░░░░░▓▓
    0x7C,  // ▓░░░░░▓▓

    // #049 Number '1' (U+0031 Digit One)
    0x00,  // ▓▓▓▓▓▓▓▓
    0x7C,  // ▓░░░░░▓▓
    
    // #050 Number '2' (U+0032 Digit Two)
    0x74,  // ▓░░░▓░▓▓
    0x5C,  // ▓░▓░░░▓▓

    // #051 Number '3' (U+0033 Digit Three)
    0x54,  // ▓░▓░▓░▓▓
    0x7C,  // ▓░░░░░▓▓

    // #052 Number '4' (U+0034 Digit Four)
    0x1C,  // ▓▓▓░░░▓▓
    0x78,  // ▓░░░░▓▓▓

    // #053 Number '5' (U+0035 Digit Five)
    0x5C,  // ▓░▓░░░▓▓
    0x74,  // ▓░░░▓░▓▓

    // #054 Number '6' (U+0036 Digit Six)
    0x78,  // ▓░░░░▓▓▓
    0x64,  // ▓░░▓▓░▓▓

    // #055 Number '7' (U+0037 Digit Seven)
    0x64,  // ▓░░▓▓░▓▓
    0x1C,  // ▓▓▓░░░▓▓

    // #056 Number '8' (U+0038 Digit Eight)
    0x6C,  // ▓░░▓░░▓▓
    0x7C,  // ▓░░░░░▓▓

    // #057 Number '9' (U+0039 Digit Nine)
    0x4C,  // ▓░▓▓░░▓▓
    0x3C   // ▓▓░░░░▓▓
    
};

static uint8_t score_txt[17] = {
    // 17x8 px image
    0x00,  // ▓▓▓▓▓▓▓▓
    0x28,  // ▓▓░▓░▓▓▓
    0x00,  // ▓▓▓▓▓▓▓▓
    0x58,  // ▓░▓░░▓▓▓
    0x78,  // ▓░░░░▓▓▓
    0x00,  // ▓▓▓▓▓▓▓▓
    0x08,  // ▓▓▓▓░▓▓▓
    0x78,  // ▓░░░░▓▓▓
    0x00,  // ▓▓▓▓▓▓▓▓
    0x78,  // ▓░░░░▓▓▓
    0x78,  // ▓░░░░▓▓▓
    0x00,  // ▓▓▓▓▓▓▓▓
    0x48,  // ▓░▓▓░▓▓▓
    0x78,  // ▓░░░░▓▓▓
    0x00,  // ▓▓▓▓▓▓▓▓
    0x74,  // ▓░░░▓░▓▓
    0x5C   // ▓░▓░░░▓▓
};

void draw_score_num(uint8_t num) {
    uint8_t *img = (uint8_t *)numbers + (num << 1);
    send_data(*img++);
    send_data(*img);
    send_data(0x0);
}

void delay(uint16_t ms) {
    do {
        _delay_ms(1);
    }
    while(--ms);
}

// Compiler will generate code that fill memory (22 bytes)
void main() __attribute__((naked, section(".init9")));
void main() {
    
    DDRB  = 0b00011110;
    PORTB = 0b00000001;
    
    init();
    
    while (true) {
        clear();
        
        set_page(3);
        set_coord(58);
        
        chars<0x0E, 0x11, 0x1D, 0x00, 0x0C, 0x12, 0x0C, 0x00, 0x01, 0x15, 0x03>::apply<send_data>();
        
        while ((PINB & 0x1) == 0);

        clear();

        { // horizon
            uint8_t fill = 128;
            while(--fill) {
                send_data(0x80);
            }
        }
        
        set_coord(40);
        for (uint8_t i = 5; i; i--) {
            set_page(i);
            chars<0b00000011, 0b00001100, 0b00110000, 0b11000000>::apply_inv<send_data>();
        }

        set_coord(68);
        for (uint8_t i = 1; i < 6; i++) {
            set_page(i);
            chars<0b00000011, 0b00001100, 0b00110000, 0b11000000>::apply<send_data>();
        }
        
        uint8_t car_current_offset = CAR_START_OFFSET;
        uint8_t car_max_offset = -CAR_MAX_OFFSET;
        uint8_t car_inc = -1;

        uint8_t barrier_side = 0xD2; // rnd
        uint8_t barrier_index = BARRIER_MIN_INDEX;
        uint8_t barrier_page = 1;
        uint8_t barrier_x = 0;

        uint8_t roadline_index = 0;
        
        uint8_t score0 = 0;
        uint8_t score1 = 0;
        uint8_t score2 = 0;
        
        uint8_t frame_time = START_FRAME_TIME;
        
        while (true) {
            delay(frame_time);
            
            set_coord_range(64, 64);
            set_page(2);
            
            { // center road line
                uint8_t t0 = *(uint8_t *)&roadline[roadline_index];
                uint8_t t1 = *((uint8_t *)&roadline[roadline_index] + 1);

                send_data(t0);
                send_data(t1);
                send_data(t1);
                send_data(t1);

                if (++roadline_index >= 4) {
                    roadline_index = 0;
                }
            }

            { // car
                uint8_t tmp = CAR_CENTER + car_max_offset;
                if (car_current_offset == tmp) {
                    if (PINB & 0x1) {
                        car_inc = -car_inc;
                        car_max_offset = -car_max_offset;
                    }
                }
                else {
                    car_current_offset += car_inc;
                }
            }

            set_page(5);
            set_coord(car_current_offset);

            if (car_current_offset < CAR_LEFT_SIDE) {
                if ((barrier_side & 0x1) && barrier_index > BARRIER_DANGER_INDEX) {
                    car_over::apply<send_data>();
                    break;
                }
                else {
                    car_side::apply<send_data>();
                }
            }
            else if (car_current_offset > CAR_RIGHT_SIDE) {
                if ((barrier_side & 0x1) == 0 && barrier_index > BARRIER_DANGER_INDEX) {
                    car_over::apply<send_data>();
                    break;
                }
                else {
                    car_side::apply_inv<send_data>();
                }
            }
            else {
                car_center::apply<send_data>();
            }
            
            { // barriers
                set_page(barrier_page);
                set_coord(barrier_x);
                
                {
                    uint8_t draw_count = barrier_index >> 4;
                    while (--draw_count) {
                        send_data(0x0);
                    }
                }
                
                if (barrier_index < BARRIER_STEP_1) {
                    barrier_index += 1;
                }
                else if (barrier_index < BARRIER_STEP_2) {
                    barrier_index += 2;
                }
                else if (barrier_index < BARRIER_STEP_4) {
                    barrier_index += 4;
                }
                else {
                    barrier_index = BARRIER_MIN_INDEX;
                    barrier_side = (barrier_side << 1) | (barrier_side >> 7);
                    
                    if (frame_time > 1) {
                        frame_time--;
                    }
                    
                    if (++score0 >= 10) {
                        score0 = 0;
                        if (++score1 >= 10) {
                            score1 = 0;
                            score2++;
                        }
                    }
                }

                uint8_t offset = barrier_index >> 2;
                uint8_t draw_count = offset >> 2;
                uint8_t page = (draw_count >> 1);
                
                barrier_x = 64 + ((barrier_side & 0x1) ? (-draw_count - page) : (1 + page));
                
                set_page(page);
                set_coord(barrier_x);
                barrier_page = page;

                while (--draw_count) {
                    send_data(0b00000001 << (offset & 7));
                }
            }
            
            set_page(7);
            set_coord(51);

            {
                uint8_t i = 16;
                do {
                    send_data(score_txt[i]);
                }
                while (i--);
            }

            draw_score_num(score2);
            draw_score_num(score1);
            draw_score_num(score0);
        }
        
        delay(2500);
    }
}
