#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdlib>
#include <bitset>
#include <cmath>

#define SSD_Command_Mode      0x00  /* C0 and DC bit are 0         */
#define SSD_Data_Mode         0x40  /* C0 bit is 0 and DC bit is 1 */

static const uint8_t SSD_Com_Display_Off = 0xae;
static const uint8_t SSD_Com_Display_On = 0xaf;
static const uint8_t SSD_Com_Set_Disp_Clk_Div_Ratio_Osc_Freq = 0xd5;
static const uint8_t SSD_Com_Set_Multiplex_Ratio = 0xa8;
static const uint8_t SSD_Com_Set_Display_Offset = 0xd3;
static const uint8_t SSD_Com_Set_Charge_Pump = 0x8d;
static const uint8_t SSD_Com_Enable_Charge_Pump = 0x14;
static const uint8_t SSD_Com_Set_Segment_Remap = 0xa1;
static const uint8_t SSD_Com_Set_COM_Out_Scan_Direct_Normal = 0xc0;
static const uint8_t SSD_Com_Set_COM_HW_Pins_Conf = 0xda;
static const uint8_t SSD_Com_Set_Contrast = 0x81;
static const uint8_t SSD_Com_Set_Pre_Charge_Period = 0xd9;
static const uint8_t SSD_Com_Entire_Display_Off = 0xa4;
static const uint8_t SSD_Com_Entire_Display_On = 0xa5;
static const uint8_t SSD_Com_Set_Memory_Adressing_Mode = 0x20;
static const uint8_t SSD_Memory_Adressing_Mode_H = 0x00;

static const uint8_t Column_Max = 127;
static const uint8_t Page_Max = 7;

class SSD1306
{
public:
    SSD1306(const std::string &device_bus, unsigned char slave_address)
        : m_device_bus(device_bus)
        , m_slave_address(slave_address)
    {
        screen_buffer[0] = SSD_Data_Mode;
        screen_buffer_new[0] = SSD_Data_Mode;
        commands_stack.push_back(SSD_Command_Mode);
    }

    void setup_connection()
    {
        m_i2c_bus_file_hand = open(m_device_bus.c_str(), O_RDWR);
        if (m_i2c_bus_file_hand < 0 )
        {
            fprintf(stderr, "Can not open I2C Bus at: %s\n", m_device_bus.c_str());
        }
        else
        {
            printf("I2C Bus available at: %s\n", m_device_bus.c_str());
        }

	    if (ioctl(m_i2c_bus_file_hand, I2C_SLAVE, m_slave_address) < 0 )
	    {
		    fprintf(stderr, "I2C slave not found under address: %#02x\n", m_slave_address);
	    }
	    else
        {
            if (write(m_i2c_bus_file_hand, nullptr, 0) != 0)
            {
                fprintf(stderr, "Failed to write to the i2c bus.\n");
            }
            else
            {
		        printf("I2C slave found under address: %#02x\n", m_slave_address);
            }
	    }
    }

    void init_screen()
    {
        append_commands(
        {
            SSD_Command_Mode,
            SSD_Com_Display_Off,
            SSD_Com_Set_Disp_Clk_Div_Ratio_Osc_Freq,
            0x80,
            SSD_Com_Set_Multiplex_Ratio,
            0x3f,
            SSD_Com_Set_Display_Offset,
            0x00,
            0x40,
            SSD_Com_Set_Charge_Pump,
            SSD_Com_Enable_Charge_Pump,
            SSD_Com_Set_COM_HW_Pins_Conf,
            0x12,
            SSD_Com_Set_Pre_Charge_Period,
            0xf1,
            SSD_Com_Set_Segment_Remap,
            SSD_Com_Set_COM_Out_Scan_Direct_Normal,
            SSD_Com_Set_Contrast,
            0xcf,
            SSD_Com_Entire_Display_Off,
            0xa1,
            0xc8,
            SSD_Com_Set_Memory_Adressing_Mode,
            SSD_Memory_Adressing_Mode_H,
            SSD_Com_Display_On,
        }
        );

        flush_commands();
        flush_buffer();
    }

    void send_command(unsigned char command)
    {
        append_commands({0x00, command});
        flush_commands();
    }

    void draw()
    {
        static const char buffer_length = 6;
        unsigned char buffer[buffer_length] = {SSD_Data_Mode, 255, 255, 255, 100, 100};
        send(buffer, buffer_length);
    }

    void draw_A()
    {
        static const char buffer_length = 7;
        unsigned char buffer[buffer_length] = {SSD_Data_Mode, 0x3C,0x12,0x12,0x12,0x3E,0x00}; 
        send(buffer, buffer_length);
    }
    
    void set_pixel(uint8_t x, uint8_t y, bool value)
    {
        const size_t byte_no = y / 8 *128 + x;
        std::bitset<8> b(screen_buffer_new[byte_no + 1]);
        b.set(y%8);
        screen_buffer_new[byte_no + 1] = (uint8_t)b.to_ulong();
    }

    void clean_screen()
    {
        std::fill(std::begin(screen_buffer_new) + 1, std::end(screen_buffer_new), 0);
        flush_buffer();
    }

    void draw_random_pixels()
    {
        for (auto begin = screen_buffer_new.begin() + 1; begin != screen_buffer_new.end(); ++begin)
        {
            *begin = rand() % 256; 
        }
        flush_buffer();
    }

    void set_draw_area(uint8_t column_start, uint8_t column_end, uint8_t page_start, uint8_t page_end)
    {
        append_commands({0x21, column_start, column_end, 0x22, page_start, page_end});
        flush_commands();
    };

    void draw_circle2(float x1,float y1,float r)
    {
        float x,y,p;
        x=0;
        y=r;
        p=3-(2*r);
        while(x<=y)
        {
            set_pixel(x1+x,y1+y,true);
            set_pixel(x1-x,y1+y,true);
            set_pixel(x1+x,y1-y,true);
            set_pixel(x1-x,y1-y,true);
            set_pixel(x1+y,y1+x,true);
            set_pixel(x1+y,y1-x,true);
            set_pixel(x1-y,y1+x,true);
            set_pixel(x1-y,y1-x,true);
            x=x+1;
            if(p<0)
            {
                p=p+4*(x)+6;
            }
            else
            {
                p=p+4*(x-y)+10;
                y=y-1;
            }
        }
    }

    void draw_filled_circle(int x0, int y0, int radius)
    {
        int x = radius;
        int y = 0;
        int xChange = 1 - (radius << 1);
        int yChange = 0;
        int radiusError = 0;

        while (x >= y)
        {
            for (int i = x0 - x; i <= x0 + x; i++)
            {
                set_pixel(i, y0 + y, true);
                set_pixel(i, y0 - y, true);
            }
            for (int i = x0 - y; i <= x0 + y; i++)
            {
                set_pixel(i, y0 + x, true);
                set_pixel(i, y0 - x, true);
            }

            y++;
            radiusError += yChange;
            yChange += 2;
            if (((radiusError << 1) + xChange) > 0)
            {
                x--;
                radiusError += xChange;
                xChange += 2;
            }
        }
    }

    void flush_buffer()
    {
        size_t no_of_diffs = 0;
        size_t min_column = Column_Max;
        size_t max_column = 0;
        size_t min_page = Page_Max;
        size_t max_page = 0;

        for (size_t column = 0; column <= Column_Max; ++column)
        {
            for (size_t page = 0; page <= Page_Max; ++page)
            {
                if (get_byte(column, page, screen_buffer_new) != get_byte(column, page, screen_buffer))
                {
                    min_column = std::min(min_column, column);
                    max_column = std::max(max_column, column);
                    min_page = std::min(min_page, page);
                    max_page = std::max(max_page, page);
                    ++no_of_diffs;
                }
            }
        }

        const size_t area = (max_column - min_column + 1) * (max_page - min_page + 1);
        std::vector<uint8_t> vec(area + 1);
        vec[0] = SSD_Data_Mode;
        size_t counter = 0;
        
        for (size_t page = min_page; page <= max_page; ++page)
        {
            for (size_t column = min_column; column <= max_column; ++column)
            { 
                vec[counter + 1] = get_byte(column, page, screen_buffer_new);
                ++counter; 
            }
        }

        set_draw_area(min_column, max_column, min_page, max_page);
        printf("Area: %u\nmin_c(%u) max_c(%u)\nmin_p(%u) max_p(%u)\n", area, min_column, max_column, min_page, max_page);
        send(vec);
        screen_buffer = screen_buffer_new;
    }

    void flush_commands()
    {
        send(commands_stack);
        commands_stack.resize(1);
    };

    void append_commands(const std::initializer_list<uint8_t> &list)
    {
        commands_stack.insert(std::end(commands_stack), list);
    }
private:
    const uint8_t& get_byte(uint8_t column, uint8_t page, const std::array<uint8_t, 128*64/8 + 1> &buffer)
    {
       return buffer[page * 128 + column + 1]; 
    }

    void send(const std::vector<uint8_t> &buffer)
    {
        send(buffer.data(), buffer.size());
    }

    void send(const uint8_t *buffer, size_t size)
    {
        if (write(m_i2c_bus_file_hand, buffer, size) != size)
        {
            printf("Failed to write to the i2c bus.\n");
        }
        else
        {
            printf("Slave write exists! %#02x \n", m_slave_address);
        }
    }

    const std::string m_device_bus;
    int m_i2c_bus_file_hand = 0;
    const unsigned char m_slave_address;
    std::array<uint8_t, 128*64/8 + 1> screen_buffer;
    std::array<uint8_t, 128*64/8 + 1> screen_buffer_new;
    std::vector<uint8_t> commands_stack;
};

int main(int argc, char *argv[])
{
    srand(time(NULL));

    SSD1306 oled = SSD1306("/dev/i2c-1", 0x3c);
    oled.setup_connection();
    if (strcmp(argv[1], "d_on") == 0)
    {
        oled.send_command(SSD_Com_Display_On);
    }
    else if (strcmp(argv[1], "d_off") == 0)
    {
        oled.send_command(SSD_Com_Display_Off);
    }
    else if (strcmp(argv[1], "d_draw") == 0)
    {
        oled.draw();
    }
    else if (strcmp(argv[1], "d_display_normal") == 0)
    {
        oled.send_command(0xa6);
    }
    else if (strcmp(argv[1], "d_init") == 0)
    {
        oled.init_screen();
    }
    else if (strcmp(argv[1], "d_cs") == 0)
    {
        oled.clean_screen();
    }
    else if (strcmp(argv[1], "d_random") == 0)
    {
        oled.draw_random_pixels();
    }
    else if (strcmp(argv[1], "d_a") == 0)
    {
        oled.draw_A();
    }
    else if (strcmp(argv[1], "d_back") == 0)
    {
        oled.set_draw_area(0, Column_Max, 0, Page_Max);
    }
    else if (strcmp(argv[1], "d_sp") == 0)
    {
        oled.clean_screen();
        oled.set_pixel(static_cast<uint8_t>(strtol(argv[2], NULL, 10)), static_cast<uint8_t>(strtol(argv[3], NULL, 10)), true);
        oled.flush_buffer();
    }
    else if (strcmp(argv[1], "d_circle") == 0)
    {
        oled.clean_screen();
        const uint8_t x = static_cast<uint8_t>(strtol(argv[2], NULL, 10));
        const uint8_t y = static_cast<uint8_t>(strtol(argv[3], NULL, 10));
        const uint8_t r = static_cast<uint8_t>(strtol(argv[4], NULL, 10));
        oled.draw_circle2(x, y, r);
        oled.flush_buffer();
    }
    else if (strcmp(argv[1], "d_circle_f") == 0)
    {
        oled.clean_screen();
        const uint8_t x = static_cast<uint8_t>(strtol(argv[2], NULL, 10));
        const uint8_t y = static_cast<uint8_t>(strtol(argv[3], NULL, 10));
        const uint8_t r = static_cast<uint8_t>(strtol(argv[4], NULL, 10));
        oled.draw_filled_circle(x, y, r);
        oled.flush_buffer();
    }
    else if (strcmp(argv[1], "d") == 0)
    {
        unsigned char num = (unsigned char)strtol(argv[2], NULL, 16);
        oled.send_command(num);
    }
}

