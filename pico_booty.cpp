#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "parallel.pio.h"

extern const uint8_t payload_start_addr, payload_end_addr;
const uint8_t *payload = &payload_start_addr;
const int payload_size = &payload_end_addr - &payload_start_addr;

namespace Pin
{
    enum : unsigned int
    {
        D0 = 0,
        D1 = 1,
        D2 = 2,
        D3 = 3,
        D4 = 4,
        D5 = 5,
        D6 = 6,
        D7 = 7,
        CS = 8,
        RD = 9,
        RESET = 10,
    };
} // namespace Pin

namespace PIOInstance
{
    PIO const c_pioParallelOut = pio0;
} // namespace PIOInstance

namespace SM
{
    constexpr unsigned int c_smParallelOut = 0;
} // namespace SM

int initDMA();
void initParallelProgram(PIO pio, uint8_t sm, uint8_t offset);

int initDMA(const volatile void *read_addr, unsigned int transfer_count)
{
    int channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(channel);

    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);

    const unsigned int parallelDREQ = PIOInstance::c_pioParallelOut == pio0 ? DREQ_PIO0_TX0 : DREQ_PIO1_TX0;
    channel_config_set_dreq(&c, parallelDREQ);

    dma_channel_configure(channel, &c, &PIOInstance::c_pioParallelOut->txf[SM::c_smParallelOut], read_addr, transfer_count, true);

    return channel;
}

void initParallelProgram(PIO pio, uint8_t sm, uint8_t offset)
{
    pio_sm_config sm_config = parallel_program_get_default_config(offset);

    sm_config_set_out_shift(&sm_config, false, false, 8);  // 8 bits out, no autopull
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX); // We don't need TX, so we can join it to RX for more space

    // CS + RD
    pio_gpio_init(pio, Pin::CS);
    pio_gpio_init(pio, Pin::RD);
    gpio_set_input_enabled(Pin::CS, true);
    gpio_set_input_enabled(Pin::RD, true);
    sm_config_set_jmp_pin(&sm_config, Pin::RD);

    // Data
    pio_sm_set_consecutive_pindirs(pio, sm, Pin::D0, 8, false);
    sm_config_set_out_pins(&sm_config, Pin::D0, 8);
    for (uint pin = Pin::D0; pin < Pin::D0 + 8; pin++)
    {
        pio_gpio_init(pio, pin);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
    }

    pio_sm_init(pio, sm, offset, &sm_config);
}

int main()
{
    set_sys_clock_khz(250000, true); // Set clock to 250MHz, rp2040 is not fast enough at 133MHz

    // Initialize stdio for debugging
    stdio_init_all();

    // Initialize reset pin
    gpio_init(Pin::RESET);
    gpio_set_dir(Pin::RESET, GPIO_IN);
    gpio_set_input_hysteresis_enabled(Pin::RESET, true);

    initParallelProgram(PIOInstance::c_pioParallelOut, SM::c_smParallelOut, pio_add_program(PIOInstance::c_pioParallelOut, &parallel_program));

    int dmaChannel = initDMA(payload, payload_size);
    if (dmaChannel < 0)
    {
        printf("Failed to initialize DMA\n");
        return 1;
    }
    printf("DMA channel %d initialized\n", dmaChannel);

    // Reset the console and start the payload out program
    gpio_set_dir(Pin::RESET, GPIO_OUT);
    gpio_put(Pin::RESET, 0);

    pio_sm_set_enabled(PIOInstance::c_pioParallelOut, SM::c_smParallelOut, true);

    sleep_ms(250); // Wait a bit for the console to reset
    gpio_set_dir(Pin::RESET, GPIO_IN);

    while (true)
    {
        sleep_ms(100); // Nothing to do
    }
}
