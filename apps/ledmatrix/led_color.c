/*
* Copyright or © or Copr. 2011, Michael Hauspie
*
* Author e-mail: michael.hauspie@lifl.fr
*
* This software is a computer program whose purpose is to design an
* efficient Web server for very-constrained embedded system.
*
* This software is governed by the CeCILL license under French law and
* abiding by the rules of distribution of free software.  You can  use,
* modify and/ or redistribute the software under the terms of the CeCILL
* license as circulated by CEA, CNRS and INRIA at the following URL
* "http://www.cecill.info".
*
* As a counterpart to the access to the source code and  rights to copy,
* modify and redistribute granted by the license, users are provided only
* with a limited warranty  and the software's author,  the holder of the
* economic rights,  and the successive licensors  have only  limited
* liability.
*
* In this respect, the user's attention is drawn to the risks associated
* with loading,  using,  modifying and/or developing or reproducing the
* software by the user in light of its specific status of free software,
* that may mean  that it is complicated to manipulate,  and  that  also
* therefore means  that it is reserved for developers  and  experienced
* professionals having in-depth computer knowledge. Users are therefore
* encouraged to load and test the software's suitability as regards their
* requirements in conditions enabling the security of their systems and/or
* data to be ensured and,  more generally, to use and operate it in the
* same conditions as regards security.
*
* The fact that you are presently reading this means that you have had
* knowledge of the CeCILL license and that you accept its terms.
*/
/*
<generator>
        <handlers init="init_led" doGet="get_led"/>
	<properties persistence="idempotent" />
	<args>
	        <arg name="color" type="str" size="7"/>
	</args>
</generator>
 */

#include <rflpc17xx/rflpc17xx.h>

#define SPI_WRITE(val) rflpc_spi_write(RFLPC_SPI1, (val))
#define SPI_ACTIVATE_LED rflpc_gpio_clr_pin(0,6)
#define SPI_DEACTIVATE_LED rflpc_gpio_set_pin(0,6)
#define SPI_WAIT_QUEUE_EMPTY while (!rflpc_spi_tx_fifo_empty(RFLPC_SPI1))

#define SPI_INIT do { \
   int spi_peripheral_clock = rflpc_clock_get_system_clock() / 8; \
   int needed_divider = spi_peripheral_clock / 125000; \
   int serial_clock_rate_divider = 1; \
   while (needed_divider / serial_clock_rate_divider > 254) \
      serial_clock_rate_divider++; \
   needed_divider /= serial_clock_rate_divider; \
   rflpc_spi_init_master(RFLPC_SPI1, RFLPC_CCLK_8, needed_divider, serial_clock_rate_divider, 8);\
   rflpc_gpio_use_pin(0, 6);\
   rflpc_gpio_set_pin_mode_output(0,6);\
   rflpc_gpio_set_pin(0,6);\
   }while(0);

#define INIT_WAIT do { \
   rflpc_timer_enable(RFLPC_TIMER3); \
   rflpc_timer_set_clock(RFLPC_TIMER3,RFLPC_CCLK/8);\
   rflpc_timer_set_pre_scale_register(RFLPC_TIMER3, rflpc_clock_get_system_clock()/8000000); /* microsecond timer */ \
   rflpc_timer_start(RFLPC_TIMER3); \
  } while (0)

#define WAIT(s) do {uint32_t start = rflpc_timer_get_counter(RFLPC_TIMER3); while ((rflpc_timer_get_counter(RFLPC_TIMER3) - start) < (s));} while(0)

static uint8_t back_buffer[64];

static inline uint8_t htoi(char hexa)
{
   if (hexa >= '0' && hexa <= '9')
      return hexa - '0';
   if (hexa >= 'a' && hexa <= 'f')
      return hexa - 'a';
   if (hexa >= 'A' && hexa <= 'F')
      return hexa - 'A';
   return 0;
}

uint8_t str_to_color(const char *color)
{
   uint8_t r, g, b;
   r = ((htoi(color[0])*16 + htoi(color[1])) >> 5) & 0x7;
   g = ((htoi(color[2])*16 + htoi(color[3])) >> 5) & 0x7;
   b = ((htoi(color[4])*16 + htoi(color[5])) >> 5) & 0x7;
   return (r << 6) | (g << 3) | b;
}

void led_matrix_display_buffer(uint8_t *buffer)
{
   int i;
   /* Assert chip select */
   SPI_ACTIVATE_LED;
   WAIT(500);
   for (i = 0 ; i < 64 ; ++i)
      SPI_WRITE(buffer[i]);
   /* wait for transfer to finish */
   SPI_WAIT_QUEUE_EMPTY;
   /* wait 0.5 more ms */
   WAIT(500);
   SPI_DEACTIVATE_LED;
}

static char init_led(void)
{
   INIT_WAIT;
   SPI_INIT;
   uint8_t color = str_to_color("FFFFFF");
   for (i = 0 ; i < 64 ; ++i)
      back_buffer[i] = color;
   led_matrix_display_buffer(back_buffer);
   return 1;
}


static char get_led(struct args_t *args)
{
   int i;
   uint8_t color = str_to_color(args->color);
   for (i = 0 ; i < 64 ; ++i)
   {
      back_buffer[i] = color;
   }
   led_matrix_display_buffer(back_buffer);
   return 1;
}