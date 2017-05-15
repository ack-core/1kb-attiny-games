
#include "oled1306_tiny.h"

#define PORT_HI(p) (PORTB |= (1 << (p)))
#define PORT_LO(p) (PORTB &= ~(1 << (p)))

namespace oled1306 {
    namespace details {
        void send_value(unsigned char value, bool is_cmd) {
            is_cmd ? PORT_LO(D_DC) : PORT_HI(D_DC);
            PORT_LO(D_CLK);
            
            for (char i = 0; i < 8; i++) {
                PORT_LO(D_DIN);
                
                if (value & 0b10000000) {
                    PORT_HI(D_DIN);
                }
                
                value <<= 1;
                
                PORT_HI(D_CLK);
                PORT_LO(D_CLK);
            }
        }

        void send_cmd(unsigned char cmd) {
            send_value(cmd, true);
        }

        void send_dat(unsigned char dat) {
            send_value(dat, false);
        }        
    }
    
    void draw_block(unsigned char page, unsigned char index, const char *bytes, unsigned char hcount, unsigned char vcount) {
        details::send_cmd(0x21);
        details::send_cmd(index);
        details::send_cmd(index + hcount - 1);
        details::send_cmd(0x22);
        details::send_cmd(page);
        details::send_cmd(page + vcount - 1);
        
        for (unsigned char i = 0; i < hcount * vcount; i++) {
            details::send_dat(bytes[i]);
        }
    }
    
    void init() {
        PORT_HI(D_CLK);
        PORT_LO(D_RES);

        for (unsigned char i = 0; i < 255; i++) {
            asm volatile("nop");
        }

        PORT_HI(D_RES);
        
        details::send_cmd(0xAE);
        details::send_cmd(0xD5);
        details::send_cmd(0x80);
        details::send_cmd(0xA8);
        details::send_cmd(0x3F);
        details::send_cmd(0x20);
        details::send_cmd(0x00);
        details::send_cmd(0xAF);
    }
    
    void shutdown() {
        details::send_cmd(0xAE);
    }    
    
    void clear() {
        details::send_cmd(0x21);
        details::send_cmd(0);
        details::send_cmd(127);
        details::send_cmd(0x22);
        details::send_cmd(0);
        details::send_cmd(7);
        
        for (unsigned char i = 0; i < 8; i++) {
            for (unsigned char c = 0; c < 128; c++) {
                details::send_dat(0x00);
            }
        }
    }
}