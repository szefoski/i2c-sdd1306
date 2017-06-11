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

struct Pixel_buffer
{
    std::array<uint8_t, 128*64/8 + 1> screen_buffer;
};

class SSD1306
{
public:
    SSD1306(const std::string &device_bus, unsigned char slave_address)
        : m_device_bus(device_bus)
        , m_slave_address(slave_address)
    {
        screen_buffer[0] = SSD_Data_Mode;
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
        clean_screen();
        size_t pixel_no = y * 128 + x;
        size_t byte_no = y / 8 *128 + x;
        printf("x(%d), y(%d), no(%d)\n", x, y, byte_no);
        screen_buffer[byte_no + 1] = 255;
        flush_buffer();
    }

    void clean_screen()
    {
        std::fill(std::begin(screen_buffer) + 1, std::end(screen_buffer), 0);
        flush_buffer();
    }

    void draw_random_pixels()
    {
        for (auto begin = screen_buffer.begin() + 1; begin != screen_buffer.end(); ++begin)
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

    void flush_buffer()
    {
        send(screen_buffer.data(), screen_buffer.size());
    };

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
        oled.set_pixel(static_cast<uint8_t>(strtol(argv[2], NULL, 10)), static_cast<uint8_t>(strtol(argv[3], NULL, 10)), true);
    }
    else if (strcmp(argv[1], "d") == 0)
    {
        unsigned char num = (unsigned char)strtol(argv[2], NULL, 16);
        oled.send_command(num);
    }
}

