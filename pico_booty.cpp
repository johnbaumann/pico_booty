#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "parallel.pio.h"

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

extern const uint8_t payload_start_addr, payload_end_addr;
const uint8_t *payload = &payload_start_addr;
const int payload_size = &payload_end_addr - &payload_start_addr;

bool resetPending = false;
uint64_t lastLowEvent = 0;

int initDMA();
void initParallelProgram(PIO pio, uint8_t sm, uint8_t offset);
void resetCallback(uint gpio, uint32_t events);

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
    sm_config_set_set_pins(&sm_config, Pin::D0, 5); // Set pin D0 to D5 for the set(pindirs) instruction
    for (uint pin = Pin::D0; pin < Pin::D0 + 8; pin++)
    {
        pio_gpio_init(pio, pin);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
    }
    sm_config_set_sideset(&sm_config, 3 + 1, true, true); // 3 bits sideset + 1 bit for SIDE_EN(optional sideset)
    sm_config_set_sideset_pin_base(&sm_config, Pin::D5);  // Set the base pin for the sideset

    pio_sm_init(pio, sm, offset, &sm_config);
}

void resetCallback(uint gpio, uint32_t events)
{
    // resetPending = true;
    if (events & GPIO_IRQ_LEVEL_LOW)
    {
        lastLowEvent = time_us_32();
        // Disable low signal edge detection
        gpio_set_irq_enabled(Pin::RESET, GPIO_IRQ_LEVEL_LOW, false);
        // Enable high signal edge detection
        gpio_set_irq_enabled(Pin::RESET, GPIO_IRQ_LEVEL_HIGH, true);
    }
    else if (events & GPIO_IRQ_LEVEL_HIGH)
    {
        // Disable the rising edge detection
        gpio_set_irq_enabled(Pin::RESET, GPIO_IRQ_LEVEL_HIGH, false);

        const uint32_t now = time_us_32();
        const uint32_t timeElapsed = now - lastLowEvent;
        if (timeElapsed >= 500U) // Debounce, only reset if the pin was low for more than 500us(.5 ms)
        {
            resetPending = true;
        }
        else
        {
            // Enable the low signal edge detection again
            gpio_set_irq_enabled(Pin::RESET, GPIO_IRQ_LEVEL_LOW, true);
        }
    }
}

int main()
{
    // Disabled for now, re-enable if payload fails to load
    // set_sys_clock_khz(250000, true); // Set clock to 250MHz

    // Initialize stdio for debugging
    stdio_init_all();

    // Initialize reset pin
    gpio_init(Pin::RESET);
    gpio_set_dir(Pin::RESET, GPIO_IN);

    initParallelProgram(PIOInstance::c_pioParallelOut, SM::c_smParallelOut, pio_add_program(PIOInstance::c_pioParallelOut, &parallel_program));

    while (true)
    {
        int dmaChannel = initDMA(payload, payload_size);
        if (dmaChannel < 0)
        {
            printf("Failed to initialize DMA\n");
            return 1;
        }

        // Reset the console and start the payload out program
        gpio_set_dir(Pin::RESET, GPIO_OUT);
        gpio_put(Pin::RESET, 0);

        pio_sm_set_enabled(PIOInstance::c_pioParallelOut, SM::c_smParallelOut, true);
        sleep_ms(250); // Wait a bit for the console to reset
        gpio_set_dir(Pin::RESET, GPIO_IN);

        while (gpio_get(Pin::RESET) == 0)
        {
            sleep_ms(1); // Wait for the reset pin to go high
        }

        // Enable an irq handler for the reset pin, only need to set the callback once
        gpio_set_irq_enabled_with_callback(Pin::RESET, GPIO_IRQ_LEVEL_LOW, true, &resetCallback);

        while (!resetPending)
        {
            sleep_ms(1); // Nothing to do
        }

        // Both irq events are disabled now per the callback logic

        while (gpio_get(Pin::RESET) == 0)
        {
            sleep_ms(1); // Wait for the reset pin to go high
        }

        printf("Resetting...\n");
        resetPending = false;

        // Reset DMA and the PIO state machine
        dma_channel_abort(dmaChannel);
        dma_channel_unclaim(dmaChannel);
        pio_sm_set_enabled(PIOInstance::c_pioParallelOut, SM::c_smParallelOut, false);
        pio_sm_clear_fifos(PIOInstance::c_pioParallelOut, SM::c_smParallelOut);
        pio_sm_restart(PIOInstance::c_pioParallelOut, SM::c_smParallelOut);
        pio_sm_set_enabled(PIOInstance::c_pioParallelOut, SM::c_smParallelOut, true);
    }
}
