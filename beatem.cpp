//
// 1KB Fighting game for Attiny13A (1024 bytes flash / 64 bytes ram) with oled display 128x64 (ssd1306)
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

// magic
//
template <uint8_t... Chars> struct chars {
private:
    template <void (*f)(uint8_t), typename = void> inline static void _apply() {}
    template <void (*f)(uint8_t), uint8_t Ch, uint8_t... Chs> inline static void _apply() {
        f(Ch);
        _apply<f, Chs...>();
    }
        
public:
    template <void (*f)(uint8_t)> static void apply() {
        _apply<f, Chars...>();
    }
        
    template <void (*f)(uint8_t)> static void __attribute__ ((noinline, used)) apply_noinline() {
        _apply<f, Chars...>();
    }
};

using fighter_stay = chars<
    // 5x8 px image
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b01101100,  // 0x6C  ▓░░▓░░▓▓
    0b00011110,  // 0x1E  ▓▓▓░░░░▓
    0b00100100,  // 0x24  ▓▓░▓▓░▓▓
    0b11001000   // 0xC8  ░░▓▓░▓▓▓
>;

using fighter_walk0 = chars<
    // 5x8 px image
    0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
    0b11001100,  // 0xCC  ░░▓▓░░▓▓
    0b00011110,  // 0x1E  ▓▓▓░░░░▓
    0b11100100,  // 0xE4  ░░░▓▓░▓▓
    0b00001000   // 0x08  ▓▓▓▓░▓▓▓
>;

using fighter_walk1 = chars<
    // 5x8 px image
    0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b11101110,  // 0xEE  ░░░▓░░░▓
    0b00000100,  // 0x04  ▓▓▓▓▓░▓▓
    0b00001000   // 0x08  ▓▓▓▓░▓▓▓
>;

using fighter_punch = chars<
    // 7x8 px image
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b01100000,  // 0x60  ▓░░▓▓▓▓▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b00101110,  // 0x2E  ▓▓░▓░░░▓
    0b11000100,  // 0xC4  ░░▓▓▓░▓▓
    0b00000100,  // 0x04  ▓▓▓▓▓░▓▓
    0b00000100   // 0x04  ▓▓▓▓▓░▓▓
>;

using fighter_kick = chars<
    // 7x8 px image
    0b00010000,  // 0x10  ▓▓▓░▓▓▓▓
    0b00001110,  // 0x0E  ▓▓▓▓░░░▓
    0b11111100,  // 0xFC  ░░░░░░▓▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b00000100,  // 0x04  ▓▓▓▓▓░▓▓
    0b00000010,  // 0x02  ▓▓▓▓▓▓░▓
    0b00000010   // 0x02  ▓▓▓▓▓▓░▓
>;

using fighter_hitted = chars<
    // 5x8 px image
    0b10011100,  // 0x9C  ░▓▓░░░▓▓
    0b01001110,  // 0x4E  ▓░▓▓░░░▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b11101000,  // 0xE8  ░░░▓░▓▓▓
    0b00000000   // 0x00  ▓▓▓▓▓▓▓▓
>;

using enemy_stay = chars<
    // 5x8 px image
    0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
    0b00101010,  // 0x2A  ▓▓░▓░▓░▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b11101100,  // 0xEC  ░░░▓░░▓▓
    0b00000000   // 0x00  ▓▓▓▓▓▓▓▓
>;

using enemy_punch = chars<
    // 6x8 px image
    0b00000100,  // 0x04  ▓▓▓▓▓░▓▓
    0b11000100,  // 0xC4  ░░▓▓▓░▓▓
    0b00100110,  // 0x26  ▓▓░▓▓░░▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b01101000,  // 0x68  ▓░░▓░▓▓▓
    0b10000000   // 0x80  ░▓▓▓▓▓▓▓
>;

using enemy_hitted = chars<
    // 6x8 px image
    0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
    0b00100000,  // 0x20  ▓▓░▓▓▓▓▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓
    0b01101110,  // 0x6E  ▓░░▓░░░▓
    0b10000100,  // 0x84  ░▓▓▓▓░▓▓
    0b00001000   // 0x08  ▓▓▓▓░▓▓▓
>;

using building0 = chars<
    // 6x8 px image
    0b11111111,  // 0xFF  ░░░░░░░░
    0b11010101,  // 0xD5  ░░▓░▓░▓░
    0b11111111,  // 0xFF  ░░░░░░░░
    0b11010101,  // 0xD5  ░░▓░▓░▓░
    0b11111111,  // 0xFF  ░░░░░░░░
    0b10000000   // 0x80  ░▓▓▓▓▓▓▓
>;

using building1 = chars<
    // 6x8 px image
    0b11111100,  // 0xFC  ░░░░░░▓▓
    0b01010100,  // 0x54  ▓░▓░▓░▓▓
    0b01111100,  // 0x7C  ▓░░░░░▓▓
    0b01010100,  // 0x54  ▓░▓░▓░▓▓
    0b11111100,  // 0xFC  ░░░░░░▓▓
    0b10000000   // 0x80  ░▓▓▓▓▓▓▓
>;

using building2 = chars<
    // 4x8 px image
    0b00110000,  // 0x30  ▓▓░░▓▓▓▓
    0b11001000,  // 0xC8  ░░▓▓░▓▓▓
    0b00100000,  // 0x20  ▓▓░▓▓▓▓▓
    0b10000000   // 0x80  ░▓▓▓▓▓▓▓
>;

using startText = chars<
    // 8x8 px image
    0b10010010,  // 0x92  ░▓▓░▓▓░▓
    0b11111111,  // 0xFF  ░░░░░░░░
    0b00001010,  // 0x0A  ▓▓▓▓░▓░▓
    0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
    0b11111110,  // 0xFE  ░░░░░░░▓
    0b01001011,  // 0x4B  ▓░▓▓░▓░░
    0b01001010,  // 0x4A  ▓░▓▓░▓░▓
    0b11111110   // 0xFE  ░░░░░░░▓
>;

const uint8_t imgdat[] = {
    
    // Enemy fall animation
    // 5x8 px images

    // Frame 0
    0b00100000,  // 0x20  ▓▓░▓▓▓▓▓
    0b00010000,  // 0x10  ▓▓▓░▓▓▓▓
    0b11110100,  // 0xF4  ░░░░▓░▓▓
    0b00111000,  // 0x38  ▓▓░░░▓▓▓
    0b00011100,  // 0x1C  ▓▓▓░░░▓▓

    // Frame 1
    0b00100000,  // 0x20  ▓▓░▓▓▓▓▓
    0b00100000,  // 0x20  ▓▓░▓▓▓▓▓
    0b01000000,  // 0x40  ▓░▓▓▓▓▓▓
    0b01100000,  // 0x60  ▓░░▓▓▓▓▓
    0b01110000,  // 0x70  ▓░░░▓▓▓▓

    // Frame 2
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
    0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓


    // Fighter fall animation
    // 5x8 px images

    // Frame 0
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b01110000,  // 0x70  ▓░░░▓▓▓▓
    0b00111000,  // 0x38  ▓▓░░░▓▓▓
    0b11011000,  // 0xD8  ░░▓░░▓▓▓
    0b00001000,  // 0x08  ▓▓▓▓░▓▓▓

    // Frame 1
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b11100000,  // 0xE0  ░░░▓▓▓▓▓
    0b01110000,  // 0x70  ▓░░░▓▓▓▓
    0b11110000,  // 0xF0  ░░░░▓▓▓▓
    0b00010000,  // 0x10  ▓▓▓░▓▓▓▓

    // Frame 2
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
    0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
    0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
    0b01000000,  // 0x40  ▓░▓▓▓▓▓▓
    0b10000000,  // 0x80  ░▓▓▓▓▓▓▓


    // Numbers
    // 3x5 px font

    // #048 Number '0' (U+0030 Digit Zero)
    0x1F,  // ▓▓▓░░░░░
    0x11,  // ▓▓▓░▓▓▓░
    0x1F,  // ▓▓▓░░░░░

    // #049 Number '1' (U+0031 Digit One)
    0x12,  // ▓▓▓░▓▓░▓
    0x1F,  // ▓▓▓░░░░░
    0x10,  // ▓▓▓░▓▓▓▓

    // #050 Number '2' (U+0032 Digit Two)
    0x1D,  // ▓▓▓░░░▓░
    0x15,  // ▓▓▓░▓░▓░
    0x17,  // ▓▓▓░▓░░░

    // #051 Number '3' (U+0033 Digit Three)
    0x11,  // ▓▓▓░▓▓▓░
    0x15,  // ▓▓▓░▓░▓░
    0x1F,  // ▓▓▓░░░░░

    // #052 Number '4' (U+0034 Digit Four)
    0x07,  // ▓▓▓▓▓░░░
    0x04,  // ▓▓▓▓▓░▓▓
    0x1F,  // ▓▓▓░░░░░

    // #053 Number '5' (U+0035 Digit Five)
    0x17,  // ▓▓▓░▓░░░
    0x15,  // ▓▓▓░▓░▓░
    0x1D,  // ▓▓▓░░░▓░

    // #054 Number '6' (U+0036 Digit Six)
    0x1F,  // ▓▓▓░░░░░
    0x15,  // ▓▓▓░▓░▓░
    0x1D,  // ▓▓▓░░░▓░

    // #055 Number '7' (U+0037 Digit Seven)
    0x01,  // ▓▓▓▓▓▓▓░
    0x1D,  // ▓▓▓░░░▓░
    0x03,  // ▓▓▓▓▓▓░░

    // #056 Number '8' (U+0038 Digit Eight)
    0x1F,  // ▓▓▓░░░░░
    0x15,  // ▓▓▓░▓░▓░
    0x1F,  // ▓▓▓░░░░░

    // #057 Number '9' (U+0039 Digit Nine)
    0x17,  // ▓▓▓░▓░░░
    0x15,  // ▓▓▓░▓░▓░
    0x1F   // ▓▓▓░░░░░
};

union compreg {
    const uint8_t *ptr;
    
    struct {
        uint8_t lo, hi;
    };
};

// does not count as assembler using :)
register compreg anim_ptr asm ("r2");
register uint8_t anim_offset asm ("r4");
register uint8_t bg_draw_offset asm ("r6");
register uint8_t range_arg_0 asm ("r16");
register uint8_t range_arg_1 asm ("r17");

static constexpr uint8_t DISPLAY_MAX_X_COORD = 127;
static constexpr uint8_t BACKGROUND_A_VERTICAL_OFFSET = 2;
static constexpr uint8_t BACKGROUND_B_VERTICAL_OFFSET = 5;
static constexpr uint8_t START_TEXT_HORIZONTAL_OFFSET = 58;
static constexpr uint8_t SCORE_HORIZONTAL_OFFSET = 120;
static constexpr uint8_t PLAYER_VERTICAL_OFFSET = 4;
static constexpr uint8_t PLAYER_HORIZONTAL_OFFSET = 43;
static constexpr uint8_t ENEMY_FIGHT_OFFSET = 50;

void send_value(uint8_t value) {
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

void __attribute__ ((noinline)) send_command(uint8_t cmd) {
    OLED1306_PORT_LO(D_DC);
    send_value(cmd);
}

void __attribute__ ((noinline)) send_data(uint8_t dat) {
    OLED1306_PORT_HI(D_DC);
    send_value(dat);
}

static void init_display() {
    OLED1306_PORT_HI(D_CLK);
    OLED1306_PORT_LO(D_RES);
    
    uint8_t i = 255;
    do {
        asm volatile("nop");
    }
    while(--i);
    
    OLED1306_PORT_HI(D_RES);
    
    chars<0xAE, 0xD5, 0x80, 0xA8, 0x3f, 0x20, 0x00, 0xAF>::apply<send_command>();
}

void set_coord_range_r_imp() {
    send_command(0x21);
    send_command(range_arg_0);
    send_command(range_arg_1); 
}

void set_page_range_r_imp() {
    send_command(0x22);
    send_command(range_arg_0);
    send_command(range_arg_1);
}

#define set_coord_range_r(s, e) do {\
    range_arg_0 = s; \
    range_arg_1 = e; \
    set_coord_range_r_imp(); } while(0);

#define set_page_range_r(s, e) do {\
    range_arg_0 = s; \
    range_arg_1 = e; \
    set_page_range_r_imp(); } while(0);

void delay(uint8_t ms_x10) {
    do {
        _delay_ms(10);
    }
    while (--ms_x10);
}

void drawFighter(uint8_t counter) {
    uint8_t frame = counter & 0b110;
            
    if (frame == 0) {
        fighter_stay::apply<send_data>();
    }
    else if (frame == 2) {
        fighter_walk0::apply_noinline<send_data>();
    }
    else if (frame == 4) {
        fighter_walk1::apply<send_data>();
    }
    else {
        fighter_walk0::apply_noinline<send_data>();
    }
}

void send_data_c(uint8_t dat) {
    if (++range_arg_0 > bg_draw_offset) {
        OLED1306_PORT_HI(D_DC);
        send_value(dat);
    }
    else {
        uint8_t i = 16;
        do {
            asm volatile("nop"); // also
        }
        while(--i);
    }
}

void fill_ground() {
    do {
        send_data_c(0b10000000);
    }
    while(--range_arg_1);
}

void fall() {
    anim_ptr.ptr = imgdat + range_arg_0;
    range_arg_0 = 3;
                            
    do {
        range_arg_1 = 5;
        
        do {
            send_data(*anim_ptr.ptr);
            anim_ptr.lo++;
        }
        while(--range_arg_1);
        delay(20);
    }
    while(--range_arg_0);
    delay(40);    
}

void draw_score() {
    range_arg_0 = 3;
    anim_ptr.ptr = imgdat + 30 + anim_offset;
        
    do {
        send_data(*anim_ptr.ptr);
        anim_ptr.lo++;
    }
    while(--range_arg_0);

    send_data(0);
}

// Compiler will generate code that fill memory (22 bytes)
void main() __attribute__((naked, section(".init9")));
void main() {
    DDRB  = 0b00011110;
    PORTB = 0b00000001;

    init_display();
    game_start:
    
    set_page_range_r(0, 7);
    range_arg_1 = DISPLAY_MAX_X_COORD;
    set_coord_range_r_imp();    

    { // clear display 64x128 (1-bit per pixel)
        uint16_t i = 8 * 128;
    
        do {
            send_data(0x00);
        }
        while (--i);    
    }    

    range_arg_0 = BACKGROUND_A_VERTICAL_OFFSET;
    set_page_range_r_imp();
    range_arg_0 = START_TEXT_HORIZONTAL_OFFSET;
    set_coord_range_r_imp();
    
    // chinese verb "beat"
    startText::apply<send_data>();
    
    // wait for button press
    while((PINB & 0x1) == 0);
            
    int8_t life = 10;
    uint8_t rnd;
    
    union {
        uint16_t word;
        struct {
            uint8_t lo;
            uint8_t hi;
        };
    } score;
    
    score.word = 0;
    
    // cycle per enemy
    while (true) {        
        uint8_t enemy_offset = 100;
        uint8_t enemy_punch_counter = 0;
        uint8_t player_punch_counter = 0;
        uint8_t player_walk_counter = 0;
        uint8_t prev_key = 0;
                
        // draw cycle
        while (true) {       
            set_page_range_r(0, 0);
            range_arg_1 = DISPLAY_MAX_X_COORD;
            set_coord_range_r_imp(); 

            // top-left life bar
            {
                int8_t i = 0;
                do {                    
                    send_data(i >= life ? 0x19 : 0x1F);
                }
                while(++i < 10);
            }

            range_arg_0 = SCORE_HORIZONTAL_OFFSET;
            set_coord_range_r_imp();

            // top-right score
            anim_offset = score.hi;
            draw_score();
            anim_offset = score.lo;
            draw_score();

            set_page_range_r(BACKGROUND_A_VERTICAL_OFFSET, BACKGROUND_A_VERTICAL_OFFSET); 
            set_coord_range_r(0, DISPLAY_MAX_X_COORD);
            
            range_arg_0 = 0;
            
            // background image
            {
                uint8_t i = 2;
                
                do {
                    building2::apply_noinline<send_data_c>();
                    building0::apply_noinline<send_data_c>();
                    building0::apply_noinline<send_data_c>();
                    building1::apply_noinline<send_data_c>();
                    range_arg_1 = 19;
                    fill_ground();
                    building2::apply_noinline<send_data_c>();
                    building0::apply_noinline<send_data_c>();
                    building2::apply_noinline<send_data_c>();
                    building2::apply_noinline<send_data_c>();
                    range_arg_1 = 8;
                    fill_ground();
                    building1::apply_noinline<send_data_c>();
                    building2::apply_noinline<send_data_c>();
                    range_arg_1 = 21;
                    fill_ground();
                    building0::apply_noinline<send_data_c>();
                    building1::apply_noinline<send_data_c>();
                    range_arg_1 = 18;
                    fill_ground();
                }
                while(--i);
            }      
            
            set_page_range_r(BACKGROUND_B_VERTICAL_OFFSET, BACKGROUND_B_VERTICAL_OFFSET);     

            range_arg_1 = range_arg_0 = 128;            
            fill_ground();
              
            set_page_range_r(PLAYER_VERTICAL_OFFSET, PLAYER_VERTICAL_OFFSET);            
            set_coord_range_r(PLAYER_HORIZONTAL_OFFSET, PLAYER_HORIZONTAL_OFFSET + 14);
            
            // clear place for player and enemy
            {
                uint8_t i = 15;
                do {
                    send_data(0);
                }
                while (--i);
            }
            
            if (enemy_offset > ENEMY_FIGHT_OFFSET) {
                ++player_walk_counter;
                
                if (player_walk_counter & 0b1) {
                    --enemy_offset;
    
                    if ((player_walk_counter & 0b110) == 0b110) {
                        bg_draw_offset = (++bg_draw_offset) & 0x7f;
                    }
                }
            }
            else {
                if (enemy_punch_counter == 0) {
                    uint8_t key = PINB & 0x1;
                    uint8_t diff = key - prev_key;
                    prev_key = key;

                    // button pressed
                    if (diff == 1) {
                        if (player_punch_counter == 0) {
                            player_punch_counter = 6;
                        }
                        else {
                            fighter_kick::apply<send_data>();
                            set_coord_range_r(ENEMY_FIGHT_OFFSET, ENEMY_FIGHT_OFFSET + 4);
                            
                            // enemy fall
                            range_arg_0 = 0;
                            fall();
                            
                            score.lo += 3;
                            if (score.lo == 30 ) {
                                score.lo = 0;
                                score.hi += 3;

                                if (score.hi == 30 ) {
                                    score.lo = 0;
                                    score.hi = 0;
                                }
                            }                          
                            
                            break;
                        }
                    }
                    else { // enemy can punch us
                        if (rnd < 20) {
                            enemy_punch_counter = 6;
                            life--;
                        }
                        
                        uint8_t h = rnd & 0x80;
                        rnd <<= 1;
                        if (h == 0) {
                            rnd ^= 43;
                        }
                    }
                }
            }
                        
            if (enemy_punch_counter) {
                enemy_punch_counter--;
                fighter_hitted::apply<send_data>();
            }
            else if (player_punch_counter > 3) {
                fighter_punch::apply<send_data>();
            }
            else {
                drawFighter(player_walk_counter);
            }
            
            range_arg_1 = DISPLAY_MAX_X_COORD;
            
            if (enemy_punch_counter > 2) {
                range_arg_0 = ENEMY_FIGHT_OFFSET - 1;
                set_coord_range_r_imp();
                enemy_punch::apply<send_data>();
            }
            else {
                range_arg_0 = enemy_offset;
                set_coord_range_r_imp();
                
                if (player_punch_counter) {
                    player_punch_counter--;
                    enemy_hitted::apply<send_data>();
                }
                else {
                    enemy_stay::apply<send_data>();
                    
                    // player fall
                    if (life <= 0) {
                        set_coord_range_r(PLAYER_HORIZONTAL_OFFSET, PLAYER_HORIZONTAL_OFFSET + 4);
                        
                        range_arg_0 = 15;
                        fall();
                        
                        goto game_start;
                    }
                }
            }
            
            delay(1);
        }
    }
}

