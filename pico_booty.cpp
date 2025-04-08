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
    PIO const parallel = pio0;
} // namespace PIOInstance

namespace SM
{
    constexpr unsigned int parallel = 0;
} // namespace SM

int initDMA();
void initPins();
void parallel_program_init(PIO pio, uint8_t sm, uint8_t offset);

int initDMA(const volatile void *read_addr, unsigned int transfer_count)
{
    int channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    const unsigned int parallelDREQ = PIOInstance::parallel == pio0 ? DREQ_PIO0_TX0 : DREQ_PIO1_TX0;
    channel_config_set_dreq(&c, parallelDREQ);
    dma_channel_configure(channel, &c, &PIOInstance::parallel->txf[SM::parallel], read_addr, transfer_count, true);

    return channel;
}

void parallel_program_init(PIO pio, uint8_t sm, uint8_t offset)
{
    pio_sm_config sm_config = parallel_program_get_default_config(offset);

    for (uint pin = Pin::D0; pin < Pin::D0 + 8; pin++)
    {
        pio_gpio_init(pio, pin);
        gpio_set_pulls(pin, false, false);
        gpio_set_input_enabled(pin, false);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
    }

    // CS + RD pins
    for (uint pin = Pin::CS; pin <= Pin::RD; pin++)
    {
        pio_gpio_init(pio, pin);
        gpio_set_pulls(pin, false, false);
        gpio_set_input_enabled(pin, true);
    }

    pio_sm_set_consecutive_pindirs(pio, sm, Pin::D0, 8, false);

    sm_config_set_out_pins(&sm_config, Pin::D0, 8);
    sm_config_set_jmp_pin(&sm_config, Pin::RD);
    sm_config_set_out_shift(&sm_config, false, false, 8);

    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);

    pio_sm_init(pio, sm, offset, &sm_config);
}

void setDataPinMode(bool output)
{
    for (unsigned int i = Pin::D0; i <= Pin::D7; i++)
    {
        gpio_set_dir(i, output ? GPIO_OUT : GPIO_IN);
    }
}

int main()
{
    // Initialize stdio for debugging
    stdio_init_all();
    sleep_ms(1000); // Wait for power to stabilize
    printf("Booty Pico Bootloader\n");

    // Initialize reset pin
    gpio_init(Pin::RESET);
    gpio_set_dir(Pin::RESET, GPIO_IN);
    gpio_set_input_hysteresis_enabled(Pin::RESET, true);

    parallel_program_init(PIOInstance::parallel, SM::parallel, pio_add_program(PIOInstance::parallel, &parallel_program));
    sleep_ms(200);

    set_sys_clock_khz(250000, true); // Set clock to 250MHz

    int bytesRead = 0;
    bool write_mode = true;

    // Copy payload to RAM
    uint8_t *ram_payload = (uint8_t *)malloc(payload_size);
    if (ram_payload == NULL)
    {
        printf("Failed to allocate RAM for payload\n");
        return 1;
    }
    memcpy(ram_payload, payload, payload_size);
    printf("Payload copied to RAM\n");
programstart:
    gpio_set_dir(Pin::RESET, GPIO_OUT);
    gpio_put(Pin::RESET, 0); // Set RESET pin low to reset the console

    int dmaChannel = initDMA(ram_payload, payload_size);
    if (dmaChannel < 0)
    {
        printf("Failed to initialize DMA\n");
        free(ram_payload);
        return 1;
    }
    printf("DMA channel %d initialized\n", dmaChannel);

    pio_sm_set_enabled(PIOInstance::parallel, SM::parallel, true);

    sleep_ms(250); // Wait for the console to reset
    gpio_set_dir(Pin::RESET, GPIO_IN);

    while (true)
    {
        sleep_ms(100); // Wait for the console to be ready
    }
}
