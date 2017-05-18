
#include "oled1306_tiny.h"

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
    }

    void send_cmd(unsigned char cmd) {
        details::send_value(cmd, true);
    }

    void send_dat(unsigned char dat) {
        details::send_value(dat, false);
    }
    
    void set_rect(char p0, char p1, char i0, char i1) {
        send_cmd(0x21);
        send_cmd(i0);
        send_cmd(i1);
        send_cmd(0x22);
        send_cmd(p0);
        send_cmd(p1);
    }

    void clear() {
        set_rect(0, 7, 0, 127);
        
        for (unsigned short i = 0; i < 8 * 128; i++) {
            send_dat(0x00);
        }
    }
}