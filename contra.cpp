/*

You can see not just a program that serves human needs,
But a bridge between engineer decisions carved in lifeless digits
and something understandable, naturally enjoyable by a human.
A game.

-------------------------------------------------------------------------------
contra-like platformer game for Atmel Attiny104 + SSD1306 Display

Toolset: Atmel Studio 7, GCC Version 5.4.0 (AVR_8_bit_GNU_Toolchain_3.6.2_1778)
Additional compiler flags: -std=c++14 -Os -mmcu=attiny104 -mtiny-stack -ffreestanding -Wno-volatile-register-var
Additional linker flags: -nostartfiles -nodefaultlibs -nostdlib -mrelax
Wiring:

                             +-----+
                       VCC --|o    |-- GND
                   [   SCK --|     |--
   ASP Programmer  [  MOSI --|     |-- DISPLAY SDA (DIN)
                   [   RST --|     |-- DISPLAY SCL (CLK)
               GND -- BTN0 --|     |--
               GND -- BTN1 --|     |-- DISPLAY D/C (DCS)
               GND -- BTN2 --|     |-- DISPLAY RST
                             +-----+

BTN0 - LEFT
BTN1 - RIGHT
BTN2 - ACTION
-------------------------------------------------------------------------------
*/

#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include <stddef.h>
#include <avr/io.h>

struct pair {
    uint8_t lo;
    uint8_t hi;
};

struct port {
    volatile uint8_t &port;
    uint8_t bit;
};

// system constants and macro

#define F_CPU 8388608           // frequency
#define UBRRVALUE 1u            // maximum BAUD for our frequency
#define PGM_DATA_OFFSET 8       // static data offset on flash
#define SRAM_DATA_OFFSET 0x40   // from datasheet

static const port port_din = { PORTB, 2 };
static const port port_clk = { PORTB, 1 };
static const port port_res = { PORTA, 6 };
static const port port_dcs = { PORTA, 7 };

#define set_bit_hi(p) (p.port |= (1 << p.bit));
#define set_bit_lo(p) (p.port &= ~(1 << p.bit));

#define is_key_left() ((PINA & 0b1000) == 0)
#define is_key_right() ((PINA & 0b10000) == 0)
#define is_key_use() ((PINA & 0b100000) == 0)

// game specific constants

static const uint8_t GAME_UNIVERSAL_CONST = 6;
static const uint8_t GAME_SCREEN_WIDTH = 128;
static const uint8_t GAME_MAP_BYTE_SIZE = 64;
static const uint8_t GAME_MAP_BLOCK_WIDTH = 16;
static const uint8_t GAME_MAP_TOP_STAGE = 2;
static const uint8_t GAME_MAP_STAGE_DIFF = 2;
static const uint8_t GAME_HP_COORD = 12;
static const uint8_t GAME_DIST_TO_ATTACK = 60;
static const uint8_t GAME_SHOTS_MAX = 6;
static const uint8_t GAME_OBJECT_INACTIVE = 255;
static const uint8_t GAME_CHAR_WIDTH = 5;
static const uint8_t GAME_CHAR_HALF = 3;
static const uint8_t GAME_ENEMIES_MAX = 3;
static const uint8_t GAME_ENEMIES_OFFSET = 0x2;
static const uint8_t GAME_BLOCK_MASK = 0x7f;
static const uint8_t GAME_SHOOTING_DELAY = 16;
static const uint8_t GAME_LADDER_DELAY = 14;
static const uint8_t GAME_DEATH_DELAY = 40;

enum SequenceDirection : uint8_t {
    FORWARD = 1,
    BACKWARD = 0xFF
};

// compile-time alphabet-compression
// compress::Worker<Strategy, Indices...> contains field 'm' that is array of 'Indices' compressed according to 'Strategy'
//
namespace compress {
    template<typename PrevState, uint8_t Index, bool isLast> struct NextState : PrevState {
        static const uint16_t tmpacc = PrevState::accumulator | (PrevState::template CompressedIndex<Index>::value << PrevState::length);
        static const uint16_t tmplen = PrevState::length + PrevState::template CompressedIndex<Index>::length;
        static const bool filled = tmplen >= 8;

        static const uint16_t accumulator = filled ? tmpacc >> 8 : tmpacc;
        static const uint16_t length = filled ? tmplen - 8 : tmplen;
        static const uint8_t output = uint8_t(tmpacc & 0xff);
        static const bool has_output = filled || isLast;
    };

    template<typename State, uint8_t...> struct Worker {};
    template<typename State, uint8_t head, uint8_t... tail> struct Worker<State, head, tail...> : Worker<NextState<State, head, sizeof...(tail) == 0>, tail...> {
        using NextStateAdv = NextState<State, head, sizeof...(tail) == 0>;

        template<typename T, bool> struct Selector {
            template<uint8_t... out> using Result = typename Worker<NextStateAdv, tail...>::template Result<out...>;
        };
        template<typename T> struct Selector<T, true> {
            template<uint8_t... out> using Result = typename Worker<NextStateAdv, tail...>::template Result<out..., NextStateAdv::output>;
        };
        template<uint8_t... out> using Result = typename Selector<NextStateAdv, NextStateAdv::has_output>::template Result<out...>;
    };
    template<typename State> struct Worker<State> {
        template<uint8_t... out> struct Result {
            static const uint8_t size = sizeof...(out);
            const uint8_t m[size] = { out... };
        };
    };
}

// 0       : x1 - the most popular element consumes one bit of data
// 1 bbbb  : x16
// max alphabet capacity: 17 bytes
//
struct CompressingStrategy {
    static const uint16_t accumulator = 0;
    static const uint16_t length = 0;

    template<bool b> struct booltype {};
    template<uint8_t index, typename = booltype<true>> struct CompressedIndex {
        static_assert(index >= 16, "index out of range");
    };
    template<uint8_t index> struct CompressedIndex < index, booltype<index < 1>> {
        static const uint8_t length = 1;
        static const uint16_t value = 0;
    };
    template<uint8_t index> struct CompressedIndex < index, booltype<index >= 1 && index < 16>> {
        static const size_t length = 5;
        static const uint16_t value = 0b1 | ((index - 1) << 1u);
    };
};

template<uint8_t> const uint8_t indexof = 0;
template<uint8_t... In> struct Encoded : compress::Worker<CompressingStrategy, indexof<In>...>::template Result<> {};

template<> const uint8_t indexof<0b00000000> = 0;
template<> const uint8_t indexof<0b01110111> = 1;
template<> const uint8_t indexof<0b01110000> = 2;
template<> const uint8_t indexof<0b00101000> = 3;
template<> const uint8_t indexof<0b11111111> = 4;
template<> const uint8_t indexof<0b01111000> = 5;
template<> const uint8_t indexof<0b00001000> = 6;
template<> const uint8_t indexof<0b01010101> = 7;
template<> const uint8_t indexof<0b00011000> = 8;
template<> const uint8_t indexof<0b00000111> = 9;
template<> const uint8_t indexof<0b11110000> = 10;
template<> const uint8_t indexof<0b01010000> = 11;
template<> const uint8_t indexof<0b00110000> = 12;
template<> const uint8_t indexof<0b01000000> = 13;
template<> const uint8_t indexof<0b00100000> = 14;

// const data in the program memory (compressed)
// data size: 192 bytes
// compressed size: 61 bytes
//
const struct TightData {
    Encoded<
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b11111111,
        0b01010101,
        0b01010101,
        0b11111111,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    >
    le;

    Encoded<
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b11110000,
        0b01010000,
        0b01010000,
        0b11110000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    >
    fn;

    Encoded<
        0b01110111,
        0b00000111,
        0b01110111,
        0b01110111,
        0b01110111,
        0b01110000,
        0b01110111,
        0b01110111,
        0b01110111,
        0b00000111,
        0b01110111,
        0b01110111,
        0b01110111,
        0b01110000,
        0b01110111,
        0b01110111
    >
    bs;

    Encoded<
        0b01110111,
        0b00000111,
        0b01110111,
        0b01110111,
        0b00000000,
        0b00000000,
        0b11111111,
        0b01010101,
        0b01010101,
        0b11111111,
        0b00000000,
        0b00000000,
        0b01110111,
        0b01110111,
        0b01110000,
        0b01110111
    >
    lb;

    Encoded<
        0b00101000,
        0b00101000,
        0b00001000,
        0b01111000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    >
    hl;

    Encoded<
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b01111000,
        0b00001000,
        0b00101000,
        0b00001000
    >
    hr;

    Encoded<
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b01110000,
        0b00100000,
        0b01110000,
        0b00000000,
        0b01110000,
        0b00110000,
        0b00000000,
        0b01110000,
        0b01110000,
        0b01110000,
        0b01110000,
        0b01110000
    >
    hp;

    Encoded<
        0b01110000,
        0b01110000,
        0b00110000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    >
    hb;

    Encoded<
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b01110000,
        0b00001000,
        0b00001000,
        0b01110000
    >
    em;

    Encoded <
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b01111000,
        0b00101000,
        0b00011000,
        0b00000000,
        0b01111000,
        0b01000000,
        0b00000000,
        0b01110000,
        0b00101000,
        0b01110000,
        0b00000000,
        0b00011000,
        0b01110000,
        0b00011000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    >
    title;
}
tight __attribute__((used, section(".init2")));

#define OFT(x) (uint8_t(reinterpret_cast<const uint8_t *>(x) - reinterpret_cast<const uint8_t *>(&tight)) + PGM_DATA_OFFSET) // evaluates absolute address of var in TightData
#define OFX(x) (OFT(x) | 0x80) // this kind of blocks cause player to stop

static const uint8_t EMPTY_BLOCK_OFT = PGM_DATA_OFFSET + 29; // compressed empty block is 2 zero bytes so I can use any 2 zero bytes from program memory
static const uint8_t EMPTY_BLOCK_OFX = EMPTY_BLOCK_OFT | 0x80;

// const data in the program memory (uncompressed)
//
const struct NormalData {
    uint8_t alphabet[15] = {
        0b00000000, // ------------------------------------------------------------------------------------------------------------------------ 120
        0b01110111, // ------------------ 18
        0b01110000, // ------------------ 18
        0b00101000, // ----- 5
        0b11111111, // ---- 4
        0b01111000, // ---- 4
        0b00001000, // ---- 4
        0b01010101, // ---- 4
        0b00011000, // ---- 4
        0b00000111, // --- 3
        0b11110000, // -- 2
        0b01010000, // -- 2
        0b00110000, // -- 2
        0b01000000, // - 1
        0b00100000, // - 1
    };

    uint8_t actor_defeat[GAME_CHAR_WIDTH * 3 - 1] = {
        // 5x8 px images

        // Frame 0
        0b00011010,  // 0x1A  ▓▓▓░░▓░▓
        0b11101100,  // 0xEC  ░░░▓░░▓▓
        0b00011100,  // 0x1C  ▓▓▓░░░▓▓
        0b00010100,  // 0x14  ▓▓▓░▓░▓▓
        0b01100000,  // 0x60  ▓░░▓▓▓▓▓

        // Frame 1
        0b00111000,  // 0x38  ▓▓░░░▓▓▓
        0b11110000,  // 0xF0  ░░░░▓▓▓▓
        0b01110000,  // 0x70  ▓░░░▓▓▓▓
        0b00110000,  // 0x30  ▓▓░░▓▓▓▓
        0b01100000,  // 0x60  ▓░░▓▓▓▓▓

        // Frame 2
        0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
        0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
        0b11000000,  // 0xC0  ░░▓▓▓▓▓▓
        0b10000000   // 0x80  ░▓▓▓▓▓▓▓
        // ha-ha
    };

    uint8_t actor_hiding[GAME_CHAR_WIDTH] = {
        // 5x8 px image
        0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
        0b10110000,  // 0xB0  ░▓░░▓▓▓▓
        0b01111000,  // 0x78  ▓░░░░▓▓▓
        0b00101100,  // 0x2C  ▓▓░▓░░▓▓
        0b11010000   // 0xD0  ░░▓░▓▓▓▓
    };

    uint8_t actor_ladder_animation[GAME_CHAR_WIDTH * 2 * 4] = {
        // 5x16 px images

        // Frame 0
        0b00000000,  // 0x00          ▓▓▓▓▓▓▓▓
        0b11111111,  // 0xFF          ░░░░░░░░
        0b01010101,  // 0x55          ▓░▓░▓░▓░
        0b01010101,  // 0x55          ▓░▓░▓░▓░
        0b10111111,  // 0xBF          ░▓░░░░░░
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11100110,  // 0xE6  ░░░▓▓░░▓
        0b00011111,  // 0x1F  ▓▓▓░░░░░
        0b00001010,  // 0x0A  ▓▓▓▓░▓░▓
        0b10110001,  // 0xB1  ░▓░░▓▓▓░

        // Frame 1
        0b00000000,  // 0x00          ▓▓▓▓▓▓▓▓
        0b01101111,  // 0x6F          ▓░░▓░░░░
        0b10010101,  // 0x95          ░▓▓░▓░▓░
        0b11010101,  // 0xD5          ░░▓░▓░▓░
        0b10111111,  // 0xBF          ░▓░░░░░░
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11101100,  // 0xEC  ░░░▓░░▓▓
        0b01000010,  // 0x42  ▓░▓▓▓▓░▓
        0b01000111,  // 0x47  ▓░▓▓▓░░░
        0b10111001,  // 0xB9  ░▓░░░▓▓░

        // Frame 2
        0b00000000,  // 0x00          ▓▓▓▓▓▓▓▓
        0b01101111,  // 0x6F          ▓░░▓░░░░
        0b11110101,  // 0xF5          ░░░░▓░▓░
        0b10100101,  // 0xA5          ░▓░▓▓░▓░
        0b00011011,  // 0x1B          ▓▓▓░░▓░░
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11101110,  // 0xEE  ░░░▓░░░▓
        0b01010001,  // 0x51  ▓░▓░▓▓▓░
        0b01010000,  // 0x50  ▓░▓░▓▓▓▓
        0b11111011,  // 0xFB  ░░░░░▓░░

        // Frame 3
        0b00000000,  // 0x00          ▓▓▓▓▓▓▓▓
        0b11000110,  // 0xC6          ░░▓▓▓░░▓
        0b00101001,  // 0x29          ▓▓░▓░▓▓░
        0b01111101,  // 0x7D          ▓░░░░░▓░
        0b10011011,  // 0x9B          ░▓▓░░▓░░
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11111110,  // 0xFE  ░░░░░░░▓
        0b01010100,  // 0x54  ▓░▓░▓░▓▓
        0b01010100,  // 0x54  ▓░▓░▓░▓▓
        0b11111011   // 0xFB  ░░░░░▓░░
    };

    uint8_t ladder_single[GAME_CHAR_WIDTH] = {
        // 5x8 px image
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11111111,  // 0xFF  ░░░░░░░░
        0b01010101,  // 0x55  ▓░▓░▓░▓░
        0b01010101,  // 0x55  ▓░▓░▓░▓░
        0b11111111   // 0xFF  ░░░░░░░░
    };

    uint8_t actor_walk[GAME_CHAR_WIDTH * 4] = {
        // 5x8 px images

        // Frame 0
        0b10000000,  // 0x80  ░▓▓▓▓▓▓▓
        0b01101110,  // 0x6E  ▓░░▓░░░▓
        0b00010011,  // 0x13  ▓▓▓░▓▓░░
        0b00100110,  // 0x26  ▓▓░▓▓░░▓
        0b11000010,  // 0xC2  ░░▓▓▓▓░▓

        // Frame 1
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11001110,  // 0xCE  ░░▓▓░░░▓
        0b00010011,  // 0x13  ▓▓▓░▓▓░░
        0b11100110,  // 0xE6  ░░░▓▓░░▓
        0b00000010,  // 0x02  ▓▓▓▓▓▓░▓

        // Frame 2
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b00001110,  // 0x0E  ▓▓▓▓░░░▓
        0b11110011,  // 0xF3  ░░░░▓▓░░
        0b00000110,  // 0x06  ▓▓▓▓▓░░▓
        0b00000010,  // 0x02  ▓▓▓▓▓▓░▓

        // Frame 3
        0b00000000,  // 0x00  ▓▓▓▓▓▓▓▓
        0b11001110,  // 0xCE  ░░▓▓░░░▓
        0b00010011,  // 0x13  ▓▓▓░▓▓░░
        0b11100110,  // 0xE6  ░░░▓▓░░▓
        0b00000010   // 0x02  ▓▓▓▓▓▓░▓
    };

    uint8_t level_0[GAME_MAP_BYTE_SIZE / 2] = { // Every even row is here. Odd rows are evaluated
        OFT(tight.hp.m), OFT(tight.hb.m), EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, OFT(tight.fn.m),
        OFX(tight.em.m), OFT(tight.le.m), EMPTY_BLOCK_OFT, OFX(tight.hr.m), OFX(tight.hl.m), EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, OFX(tight.le.m),
        EMPTY_BLOCK_OFX, OFX(tight.le.m), EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, OFX(tight.hr.m), OFX(tight.hl.m), OFT(tight.le.m), EMPTY_BLOCK_OFX,
        EMPTY_BLOCK_OFX, EMPTY_BLOCK_OFT, OFX(tight.hr.m), OFX(tight.hl.m), EMPTY_BLOCK_OFT, EMPTY_BLOCK_OFT, OFX(tight.le.m), OFX(tight.em.m),
    };
}
normal __attribute__((used, section(".init3")));

#define OFS(x) (uint8_t(reinterpret_cast<const uint8_t *>(x) - reinterpret_cast<const uint8_t *>(&normal)) + PGM_DATA_OFFSET + sizeof(tight)) // evaluates absolute address of var in NormalData
#define OFN(x) (uint8_t(reinterpret_cast<const uint8_t *>(x) - reinterpret_cast<const uint8_t *>(&normal)) + PGM_DATA_OFFSET + sizeof(tight) - 1) // see spi::send_dat_sequence for details

// Enemies placed to .init1 so enemy description offset is equal to it's vertical position (optimization)
//
struct Enemy {
    uint8_t h_position;
    uint8_t direction;
}
enemies[GAME_ENEMIES_MAX] __attribute__((used, section(".init1"))) = {
    {101, SequenceDirection::BACKWARD},
    {37, SequenceDirection::FORWARD},
    {85, SequenceDirection::BACKWARD},
};

// dynamic SRAM data
//
struct {
    uint8_t time_counter;
    uint8_t draw_x_coord;
    uint8_t attack_counter;
    uint8_t player_position_h;
    uint8_t player_position_v;
    uint8_t player_img_position;
    uint8_t player_health;
    uint8_t enemy_position_h;
    uint8_t enemy_position_v;
    uint8_t enemy_v_last;
    uint8_t enemy_state;
    uint8_t enemy_img_offset;
    uint8_t shots_coords[GAME_SHOTS_MAX];
    uint8_t shots_dirs[GAME_SHOTS_MAX];
}
dynamic __attribute__((used));
static_assert(sizeof(dynamic) <= 24, "Out of SRAM"); // 8 bytes of SRAM left for the callstack

#define OFD(x) (uint8_t(reinterpret_cast<const uint8_t *>(x) - reinterpret_cast<const uint8_t *>(&dynamic)) + SRAM_DATA_OFFSET) // evaluates absolute address of var in SRAM

// SRAM access intrinsics
//
#define mem_load(var) ({                                                 \
    uint8_t result;                                                      \
    asm volatile (                                                       \
        "lds %0, %1\n\t"                                                 \
        : "=r"(result)                                                   \
        : "X"(SRAM_DATA_OFFSET + uint16_t(&var) - uint16_t(&dynamic))    \
    );                                                                   \
    result;                                                              \
})
#define mem_load_inc(var, value) ({                                      \
    uint8_t result;                                                      \
    asm volatile (                                                       \
        "lds  %0, %1\n\t"                                                \
        "subi %0, %2\n\t"                                                \
        : "=r"(result)                                                   \
        : "X"(SRAM_DATA_OFFSET + uint16_t(&var) - uint16_t(&dynamic))    \
        , "M"(256 - value)                                               \
    );                                                                   \
    result;                                                              \
})
#define mem_load_reg(var, result) ({                                     \
    asm volatile (                                                       \
        "lds %0, %1\n\t"                                                 \
        : "=r"(result)                                                   \
        : "X"(SRAM_DATA_OFFSET + uint16_t(&var) - uint16_t(&dynamic))    \
    );                                                                   \
})
#define mem_store_reg(var, value) ({                                     \
    uint8_t tmp = uint8_t(value);                                        \
    asm volatile (                                                       \
        "sts %0, %1\n\t"                                                 \
        :: "X"(SRAM_DATA_OFFSET + uint16_t(&var) - uint16_t(&dynamic))   \
        , "r"(tmp)                                                       \
    );                                                                   \
})

// Some player states is equal to the corresponding block offset
//
enum PlayerState : uint8_t {
    IDLE = EMPTY_BLOCK_OFX,
    IDLE_LADDER = OFX(tight.le.m),
    OBSTACLE_LEFT = OFX(tight.hl.m),
    OBSTACLE_RIGHT = OFX(tight.hr.m),
    WAIT = 0xF1,
    WALK = 0xF2,
    WALK1 = 0xF3,
    DEATH = 0xF4,
};

// permanent register binding for global variables which are often used
//
volatile register uint8_t sequence_direction asm("r16");
volatile register uint8_t player_img_offset asm("r17"); // compiler assumes that r17 is always zero. Not so wise for a core with reduced register file (only 16 registers available).
volatile register uint8_t player_state asm("r18"); // so I have to avoid using zeros in the code. And, of course, check the generated assembly for r17 and r18

// general intrinsics
//
#define lpminc_y(addr) ({                                                \
    uint8_t result;                                                      \
    asm volatile (                                                       \
        "ld %0, %a1+\n\t"                                                \
        : "=r"(result)                                                   \
        , "+y"(addr)                                                     \
    );                                                                   \
    result;                                                              \
})
#define lpm_z(addr, result) ({                                           \
    asm volatile (                                                       \
        "ld %0, %a1\n\t"                                                 \
        : "=r"(result)                                                   \
        : "z"(addr)                                                      \
    );                                                                   \
})
#define lpminc_z(addr, result) ({                                        \
    asm volatile (                                                       \
        "ld %0, %a1+\n\t"                                                \
        : "=r"(result)                                                   \
        , "+z"(addr)                                                     \
    );                                                                   \
})
#define lpminc_zw(out, addr) ({                                          \
    asm volatile (                                                       \
        "ld %A0, %a1+\n\t"                                               \
        "ld %B0, %a1\n\t"                                                \
        : "=r"(out)                                                      \
        , "+z"(addr)                                                     \
    );                                                                   \
})
#define lpmincv_z(addr, result, increment) ({                            \
    asm volatile (                                                       \
        "ld %0, %a1\n\t"                                                 \
        "subi r30, %2\n\t"                                               \
        : "=r"(result)                                                   \
        , "+z"(addr)                                                     \
        : "M"(256 - increment)                                           \
    );                                                                   \
})
#define stm_z(addr, value) ({                                            \
    asm volatile (                                                       \
        "st %a0, %1\n\t"                                                 \
        : "+z"(addr)                                                     \
        : "r"(value)                                                     \
    );                                                                   \
})
#define stminc_z(addr, value) ({                                         \
    asm volatile (                                                       \
        "st %a0+, %1\n\t"                                                \
        : "+z"(addr)                                                     \
        : "r"(value)                                                     \
    );                                                                   \
})
#define stmdec_z(addr, value) ({                                         \
    asm volatile (                                                       \
        "st -%a0, %1\n\t"                                                \
        : "+z"(addr)                                                     \
        : "r"(value)                                                     \
    );                                                                   \
})
#define addreg(dest, reg) ({                                             \
    asm volatile (                                                       \
        "add %0, %1\n\t"                                                 \
        : "=r"(dest)                                                     \
        : "r"(reg)                                                       \
    );                                                                   \
})
#define addval(dest, value) ({                                           \
    asm volatile (                                                       \
        "subi %0, %1\n\t"                                                \
        : "=r"(dest)                                                     \
        : "M"(256 - value)                                               \
    );                                                                   \
})

// input/output support (hardware SPI is used)
//
namespace spi {
    extern void send_sync(void) asm("send_sync"); // make this function available to call from asm

    void send_sync() __attribute__((section(".init6")));
    void send_sync() {
        while (!(UCSRA & (1 << TXC)));
        UCSRA = 1 << TXC;
    }

    void send_dat_sequence(uint8_t offset, uint8_t count) __attribute__((naked, section(".init5")));
    void send_dat_sequence(uint8_t offset, uint8_t count) { // unified function to send compressed and uncompressed data
        set_bit_hi(port_dcs);

        uint8_t counter = 8;
        uint8_t inc;

        union {
            struct {
                uint8_t lo;
                uint8_t hi;
            };

            uint16_t word;
        }
        input;

        do {
            volatile register pair ptr asm("r30") = { offset, __AVR_TINY_PM_BASE_ADDRESS__ >> 8 };

            if (counter >= 8) { // offset will be increased before drawing so OFN(...) is used instead of OFS(...)
                counter -= 8;

                lpminc_zw(input.word, ptr);
                inc = counter;
                offset++;
            }

            if (offset > sizeof(tight) + PGM_DATA_OFFSET) { // all data after 'tight' are uncompressed and could be written in both directions
                addreg(offset, sequence_direction);
            }
            else {
                ptr.lo = OFS(normal.alphabet); // decompressing
                input.word >>= inc;
                inc = 1;

                if (input.lo & 0b1) {
                    inc = 5;
                    ptr.lo += ((input.lo / 2) & 0b1111) + 1;
                }
            }

            uint8_t value;
            uint8_t draw_position;

            lpm_z(ptr, value);

            { // shots drawing is right here
                const uint8_t SHOTS_COORD_OFFSET = SRAM_DATA_OFFSET + uint8_t(uint16_t(dynamic.shots_coords) - uint16_t(&dynamic));
                ptr.lo = SHOTS_COORD_OFFSET;

                while (ptr.lo != SHOTS_COORD_OFFSET + GAME_SHOTS_MAX) {
                    ptr.hi = 0;

                    asm volatile ( // I spent a lot of time trying to avoid assebler here. GCC just can't do that in the way I want
                        "ld    %0, %a1+\n\t"
                        "mov   r31, %0\n\t"
                        "lds   %0, %3\n\t"
                        "cpse  %0, r31\n\t"
                        "rjmp  .+2\n\t"
                        "ori   %2, 0x02\n\t"
                        : "=r"(draw_position)
                        , "+z"(ptr)
                        , "+r"(value)
                        : "X"(SRAM_DATA_OFFSET + uint16_t(&dynamic.draw_x_coord) - uint16_t(&dynamic))
                        );
                }

                mem_store_reg(dynamic.draw_x_coord, ++draw_position);
            }

            counter += inc;

            while (!(UCSRA & (1 << UDRE)));
            UCSRA = 1 << TXC;
            UDR = value;
        }
        while (--count);
    }

    void send_cmd_seq_3(const pair p, uint8_t r) {
        set_bit_lo(port_dcs);
        UDR = p.hi;
        asm("rcall send_sync");
        UDR = p.lo;
        asm("rcall send_sync");
        UDR = r;
        send_sync();
    }
}

// additional support
//
namespace lib {
    void delay(uint8_t count) { // 'single frame calibrated pause' x count
        asm volatile (
            "ldi   r20, 255\n\t"
            "ldi   r21, 106\n\t"
            "subi  r20, 0x01\n\t"
            "sbci  r21, 0x00\n\t"
            "brne  .-6\n\t"
            "subi  %0, 0x01\n\t"
            "brne  .-14\n\t"
            :: "r"(count)
            );
    }

    void set_coord(uint8_t v, uint8_t h) {
        mem_store_reg(dynamic.draw_x_coord, h);
        spi::send_cmd_seq_3({ v, 0x22 }, { v });
        spi::send_cmd_seq_3({ mem_load(dynamic.draw_x_coord), 0x21 }, { 127 });
    }
}

// game functions
//
void draw_block(uint8_t block) {
    spi::send_dat_sequence(block & GAME_BLOCK_MASK, GAME_MAP_BLOCK_WIDTH);
}

void draw_according_direction(uint8_t offset) {
    if (sequence_direction & 0x80) {
        offset += GAME_CHAR_WIDTH - 1;
    }

    spi::send_dat_sequence(offset, GAME_CHAR_WIDTH);
}

void draw_player(uint8_t v_pos) {
    lib::set_coord(v_pos, mem_load(dynamic.player_img_position));
    draw_according_direction(player_img_offset);
}

void set_walking_state() {
    player_img_offset = OFN(normal.actor_walk);
    player_state = PlayerState::WALK1;
    sequence_direction = SequenceDirection::FORWARD;
}

void add_shot(uint8_t position) {
    volatile register pair ptr asm("r30") { OFD(dynamic.shots_coords), 0 };
    uint8_t coord;

    if (!(sequence_direction & 0x80))
    {
        position += GAME_CHAR_WIDTH;
    }

    while (ptr.lo != OFD(dynamic.shots_coords) + GAME_SHOTS_MAX) {
        lpminc_z(ptr, coord);

        if (coord > GAME_SCREEN_WIDTH) {
            stmdec_z(ptr, position);
            ptr.lo += GAME_SHOTS_MAX;
            stm_z(ptr, sequence_direction);
            break;
        }
    }
}

uint8_t dist_to_player(uint8_t coord) {
    uint8_t result = coord - mem_load_inc(dynamic.player_position_h, GAME_CHAR_HALF);
    if (result & 0x80) result = -result;
    return result;
}

extern void game(void) asm("game");

alignas(2)
void game() __attribute__((naked, noreturn, section(".init4")));
void game(void) {
    CCP = 0xD8; // 8MHz freq
    CLKPSR = 0x1;

    UCSRC = 0b11000000; // MSPIM (Tx only)
    UCSRB = (1 << TXEN);
    UBRRL = UBRRVALUE;

    DDRA = 0b11000000; // CLK, RES, DCS, Buttons
    DDRB = 0b11111111;
    PUEA = 0b00111000;
    PORTA = 0b0111000;

    set_bit_hi(port_clk); // Display reset
    set_bit_lo(port_res);

    volatile register pair sram_ptr asm("r30") { OFD(&dynamic.attack_counter), 0 }; // use pause before desplay reset to fill SRAM with default values
    uint8_t fill_value = GAME_UNIVERSAL_CONST;

    do {
        stminc_z(sram_ptr, fill_value);
    }
    while (sram_ptr.lo != OFD(&dynamic.enemy_img_offset));

    fill_value = OFN(normal.actor_walk); // big enough to fill shot coords

    do {
        stminc_z(sram_ptr, fill_value);
    }
    while (sram_ptr.lo != OFD(dynamic.shots_dirs));

    set_bit_hi(port_res);
    spi::send_cmd_seq_3({ 0x00, 0x20 }, 0xAF);

    uint8_t map_block_counter = GAME_MAP_BYTE_SIZE - 1; // title screen

    do {
        volatile uint8_t offset = EMPTY_BLOCK_OFT;
        volatile uint8_t length = GAME_MAP_BLOCK_WIDTH;

        mem_store_reg(dynamic.draw_x_coord, length); // disable shot logic for that drawing

        if (map_block_counter == 36) {
            offset = OFT(tight.title.m);
            length = 2 * GAME_MAP_BLOCK_WIDTH;
        }

        spi::send_dat_sequence(offset, length);
    }
    while (--map_block_counter);
    while (is_key_use() == 0); // press 'action' to play

    pair map_ptr = { OFS(normal.level_0), __AVR_TINY_PM_BASE_ADDRESS__ >> 8 }; // draw the level
    register uint8_t map_ladder_offset asm("r16"); // binded register isn't used at this time so use it

    while (map_block_counter < GAME_MAP_BYTE_SIZE) {
        volatile uint8_t block = OFT(tight.lb.m);
        mem_store_reg(dynamic.draw_x_coord, block); // disable shot logic for that drawing (any value that big enough)

        if ((map_block_counter & 0b1000) == 0) {
            block = lpminc_y(map_ptr);

            if (block <= OFT(tight.fn.m)) { // at even row I save ladder position
                map_ladder_offset = map_block_counter;
                addval(map_ladder_offset, 8);
            }
        }
        else if (map_ladder_offset != map_block_counter) {
            block = OFT(tight.bs.m);
        }

        draw_block(block);
        map_block_counter++;
    }

    set_walking_state();

    while (true) { // main loop

        volatile register pair shots_ptr asm("r30") { OFD(dynamic.shots_coords), 0 }; // update flying shots
        uint8_t shots_count = GAME_SHOTS_MAX;

        do {
            uint8_t inc;
            volatile uint8_t coord;

            lpmincv_z(shots_ptr, coord, GAME_SHOTS_MAX);
            lpmincv_z(shots_ptr, inc, (256 - GAME_SHOTS_MAX));

            if (coord <= GAME_SCREEN_WIDTH) {
                coord += inc;

                if (mem_load(dynamic.enemy_position_v) == mem_load(dynamic.player_position_v)) { // enemy hit
                    if (coord == mem_load_inc(dynamic.enemy_position_h, GAME_CHAR_HALF)) {
                        coord = GAME_OBJECT_INACTIVE;

                        if (mem_load(dynamic.enemy_state) == coord) {
                            mem_store_reg(dynamic.enemy_img_offset, OFN(normal.actor_defeat));
                        }

                        mem_store_reg(dynamic.enemy_state, coord);
                    }
                }
                if (dist_to_player(coord) <= 1) { // player hit
                    if (player_img_offset != OFN(normal.actor_hiding)) {
                        coord = GAME_OBJECT_INACTIVE;
                        uint8_t player_health = mem_load_inc(dynamic.player_health, uint8_t(-2));
                        mem_store_reg(dynamic.player_health, player_health);

                        if (player_health < 2) {
                            player_state = PlayerState::DEATH;
                            player_img_offset = OFN(normal.actor_defeat);
                        }

                        lib::set_coord(0, GAME_HP_COORD + player_health);

                        set_bit_hi(port_dcs); // health bar decreasing
                        UDR = 0b01010000;
                        spi::send_sync();
                        UDR = 0b01010000;
                        spi::send_sync();
                    }
                }
            }

            stminc_z(shots_ptr, coord);
        }
        while (--shots_count);

        if (player_state < PlayerState::WALK) { // player state-machine
            player_img_offset = OFN(normal.actor_walk);

            do {
                if (player_state == PlayerState::OBSTACLE_RIGHT) {
                    if (sequence_direction == FORWARD) {
                        goto obstacle_state_update;
                    }

                    player_state = PlayerState::WALK1;
                }
                if (player_state == PlayerState::OBSTACLE_LEFT) {
                    if (sequence_direction == BACKWARD) {
                        goto obstacle_state_update;
                    }

                    player_state = PlayerState::WALK1;
                }

                break;
            obstacle_state_update:
                uint8_t counter = mem_load(dynamic.attack_counter);

                if (counter) { // attack delay
                    mem_store_reg(dynamic.attack_counter, counter - 1);
                }
                else {
                    if (is_key_use()) {
                        mem_store_reg(dynamic.attack_counter, GAME_SHOOTING_DELAY);
                        add_shot(mem_load(dynamic.player_position_h));
                    }
                    else {
                        player_img_offset = OFN(normal.actor_hiding);
                    }
                }
            }
            while (false);

            volatile uint8_t player_position_h = mem_load(dynamic.player_position_h);

            if (player_position_h > GAME_MAP_BLOCK_WIDTH) {
                if (is_key_left()) {
                    set_walking_state();
                    sequence_direction = SequenceDirection::BACKWARD;
                }
            }

            if (player_position_h < GAME_SCREEN_WIDTH - GAME_MAP_BLOCK_WIDTH) {
                if (is_key_right()) {
                    set_walking_state();
                }
            }

            if (player_state == PlayerState::IDLE_LADDER) {
                if (is_key_use()) {
                    uint8_t player_position_v = mem_load(dynamic.player_position_v); // climbing isn't actual state because it stops all other things in the game
                    uint8_t repeated = 0;

                    while (true) {
                        player_img_offset = OFN(normal.actor_ladder_animation);

                        while (player_img_offset != OFN(normal.actor_ladder_animation) + sizeof(normal.actor_ladder_animation)) {
                            draw_player(player_position_v - 1);
                            addval(player_img_offset, GAME_CHAR_WIDTH);

                            draw_player(player_position_v);
                            addval(player_img_offset, GAME_CHAR_WIDTH);

                            lib::delay(GAME_LADDER_DELAY);
                        }

                        if (player_position_v == GAME_MAP_TOP_STAGE) { // restart (win)
                            asm volatile("rjmp game");
                        }

                        player_img_offset = OFN(normal.ladder_single);
                        draw_player(player_position_v--);

                        if (repeated & 0b1) {
                            break;
                        }

                        repeated++;
                    }

                    mem_store_reg(dynamic.player_position_v, player_position_v);
                    player_img_offset = OFN(normal.actor_walk);
                    player_state = PlayerState::WAIT;
                }
            }
        }

        uint8_t time_counter = mem_load_inc(dynamic.time_counter, 1);
        mem_store_reg(dynamic.time_counter, time_counter);

        if ((time_counter & 0b11) == 0) {
            if (player_state == PlayerState::WALK1) {
                player_state = PlayerState::WALK;
            }
            if (player_state == PlayerState::WALK) { // walking animation and moving
                addval(player_img_offset, GAME_CHAR_WIDTH);

                if (player_img_offset == OFN(normal.actor_walk) + 3 * GAME_CHAR_WIDTH) {
                    player_img_offset = OFN(normal.actor_walk);
                }

                uint8_t position = mem_load(dynamic.player_position_h) + sequence_direction;
                mem_store_reg(dynamic.player_position_h, position);

                if (sequence_direction & 0x80) { // == SequenceDirection::BACKWARD
                    position++;
                }

                mem_store_reg(dynamic.player_img_position, position);
            }
        }

        lib::set_coord(mem_load(dynamic.player_position_v), 0);

        uint8_t block_offset = OFS(normal.level_0) + mem_load(dynamic.player_position_v) * 4;
        uint8_t block_count = GAME_CHAR_WIDTH;

        while (block_count < GAME_SCREEN_WIDTH + GAME_CHAR_WIDTH) {
            uint8_t block;
            register pair map_addr asm("r30") = { block_offset, __AVR_TINY_PM_BASE_ADDRESS__ >> 8 };

            lpm_z(map_addr, block);
            draw_block(block);

            uint8_t player_position_h = mem_load(dynamic.player_position_h);

            if (player_state < PlayerState::WALK1) {
                if ((player_position_h == block_count) && (block & 0x80)) { // change state according to the block
                    player_state = block;
                }
            }

            block_count += GAME_MAP_BLOCK_WIDTH;
            block_offset++;
        }

        draw_player(mem_load(dynamic.player_position_v)); // redraw player

        uint8_t enemy_offset = GAME_ENEMIES_OFFSET; // drawing enemies
        uint8_t saved_direction = sequence_direction;

        while (true) {
            volatile register pair ptr asm("r30") = { enemy_offset, __AVR_TINY_PM_BASE_ADDRESS__ >> 8 };
            uint8_t pos;

            lpminc_z(ptr, pos);
            mem_store_reg(dynamic.enemy_position_h, pos);

            lpminc_z(ptr, sequence_direction);
            lib::set_coord(enemy_offset, pos);

            uint8_t last_v = mem_load(dynamic.enemy_v_last);

            if (enemy_offset > last_v) break; // all enemies have been killed
            if (enemy_offset == last_v) {
                uint8_t img = mem_load(dynamic.enemy_img_offset);
                draw_according_direction(img);

                if (img != OFN(normal.actor_walk)) {  // enemy death
                    enemy_offset -= GAME_MAP_STAGE_DIFF;
                    mem_store_reg(dynamic.enemy_position_v, enemy_offset);

                    if ((mem_load(dynamic.time_counter) & 0b111) == 0) {
                        img += GAME_CHAR_WIDTH;
                        mem_store_reg(dynamic.enemy_img_offset, img);

                        if (img >= OFN(normal.actor_defeat) + GAME_CHAR_WIDTH * 3) {
                            mem_store_reg(dynamic.enemy_img_offset, OFN(normal.actor_walk));
                            mem_store_reg(dynamic.enemy_v_last, enemy_offset);
                            mem_store_reg(dynamic.enemy_state, enemy_offset); // any that isn't equal to GAME_OBJECT_INACTIVE
                        }
                    }
                }
                if ((mem_load(dynamic.time_counter) & 0b11111) == 0) { // enemy shooting
                    if (enemy_offset == mem_load(dynamic.player_position_v)) {
                        if (dist_to_player(mem_load(dynamic.enemy_position_h)) < GAME_DIST_TO_ATTACK) {
                            add_shot(mem_load(dynamic.enemy_position_h));
                        }
                    }
                }

                break;
            }

            draw_according_direction(OFN(normal.actor_walk));
            enemy_offset += sizeof(Enemy);
        }

        lib::delay(1);

        if (player_state == PlayerState::DEATH) { // restart (lose)
            lib::delay(GAME_DEATH_DELAY);
            asm volatile("rjmp game");
        }

        sequence_direction = saved_direction;

    } // main loop
}

void main() __attribute__((naked, noreturn, section(".init0")));
void main(void) {
    asm("rjmp game");
}

// kas-shaman 2020
