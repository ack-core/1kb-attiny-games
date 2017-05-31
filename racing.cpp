#define F_CPU 1000000L

#include <avr/io.h>
#include <util/delay.h>

#include "oled1306_tiny.h"

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

using car_side = oled1306::chars<
    0b00000000,
    0b01110000,
    0b11101100,
    0b11100110,
    0b10100111,
    0b00100111,
    0b10100111,
    0b11100111,
    0b11100111,
    0b01111111,
    0b01100110,
    0b00000000
>;

using car_center = oled1306::chars<
    0b00000000,
    0b01111000,
    0b11101110,
    0b11100111,
    0b10100111,
    0b00100111,
    0b10100111,
    0b11100111,
    0b11101110,
    0b01111000,
    0b00000000,
    0b00000000
>;

using car_over = oled1306::chars<
    0b00010100,
    0b11011001,
    0b11011110,
    0b00100110,
    0b11111010,
    0b10110011,
    0b00100111,
    0b11110101,
    0b01111110,
    0b11011100,
    0b11001010,
    0b00010000
>;

static uint16_t roadline[4] = {
    0b0011001101001010,
    0b0110011001010101,
    0b1100110010101010,
    0b1001100110110101,
};

static uint8_t numbers[20] = {
    0x7C,
    0x7C,
    0x00,
    0x7C,
    0x74,
    0x5C,
    0x54,
    0x7C,
    0x1C,
    0x78,
    0x5C,
    0x74,
    0x78,
    0x64,   
    0x64,
    0x1C,
    0x6C,
    0x7C,
    0x4C,
    0x3C,
};

static uint8_t score_txt[17] = {
    0x00,
    0x28,
    0x00,
    0x58,
    0x78,
    0x00,
    0x08,
    0x78,
    0x00,
    0x78,
    0x78,
    0x00,
    0x48,
    0x78,
    0x00,
    0x74,
    0x5C,
};

void draw_score_num(uint8_t num) {
    uint8_t *img = (uint8_t *)numbers + (num << 1);
    oled1306::send_data(*img++);
    oled1306::send_data(*img);
    oled1306::send_data(0x0);
}

void delay(uint16_t ms) {
    do {
        _delay_ms(1);
    }
    while(--ms);
}

int main(void) {    
    DDRB  = 0b00011110;
    PORTB = 0b00000001;
    
    oled1306::init();
        
    while (true) {
        oled1306::clear();    
        oled1306::set_page(3);
        oled1306::set_coord(58);    
        oled1306::draw_text4x(txt("Go?"));      
              
        while ((PINB & 0x1) == 0);        
        oled1306::clear();

        { // horizon
            uint8_t fill = 128;
            while(--fill) {
                oled1306::send_data(0x80);
            }
        }
    
        oled1306::set_coord(40);
        for (uint8_t i = 5; i; i--) {
            oled1306::set_page(i);
            oled1306::chars<0b00000011, 0b00001100, 0b00110000, 0b11000000>::apply_inv<oled1306::send_data>();
        }

        oled1306::set_coord(68);
        for (uint8_t i = 1; i < 6; i++) {
            oled1306::set_page(i);
            oled1306::chars<0b00000011, 0b00001100, 0b00110000, 0b11000000>::apply<oled1306::send_data>();
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
            
            oled1306::set_coord_range(64, 64);
            oled1306::set_page(2);
            
            { // center road line
                uint8_t t0 = *(uint8_t *)&roadline[roadline_index];
                uint8_t t1 = *((uint8_t *)&roadline[roadline_index] + 1);

                oled1306::send_data(t0);
                oled1306::send_data(t1);
                oled1306::send_data(t1);
                oled1306::send_data(t1);

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

            oled1306::set_page(5);
            oled1306::set_coord(car_current_offset);

            if (car_current_offset < CAR_LEFT_SIDE) {
                if ((barrier_side & 0x1) && barrier_index > BARRIER_DANGER_INDEX) {
                    car_over::apply<oled1306::send_data>();
                    break;
                }
                else {
                    car_side::apply<oled1306::send_data>();
                }
            }
            else if (car_current_offset > CAR_RIGHT_SIDE) {
                if ((barrier_side & 0x1) == 0 && barrier_index > BARRIER_DANGER_INDEX) {
                    car_over::apply<oled1306::send_data>();
                    break;
                }
                else {
                    car_side::apply_inv<oled1306::send_data>();
                }
            }
            else {
                car_center::apply<oled1306::send_data>();
            }
        
            { // barriers
                oled1306::set_page(barrier_page);
                oled1306::set_coord(barrier_x);
                
                {
                    uint8_t draw_count = barrier_index >> 4;
                    while (--draw_count) {
                        oled1306::send_data(0x0);
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
                
                oled1306::set_page(page);
                oled1306::set_coord(barrier_x);
                barrier_page = page;

                while (--draw_count) {
                    oled1306::send_data(0b00000001 << (offset & 7));
                }
            }
        
            oled1306::set_page(7);
            oled1306::set_coord(51);

            {
                uint8_t i = 16;
                do {
                    oled1306::send_data(score_txt[i]);                
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

