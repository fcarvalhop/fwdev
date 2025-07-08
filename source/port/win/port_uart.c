#include <libserialport.h>
#include "hal.h"
#include "app.h"

#define MAX_UART_PORTS  HAL_UART_NUM_PORTS

static hal_uart_dev_t uart_devices[MAX_UART_PORTS] = { NULL };

struct hal_uart_dev_s
{
    struct sp_port* port;
    char* port_name;
    bool is_open;
    hal_uart_config_t config;
    hal_uart_interrupt_t interrupt_handler;
    hal_uart_port_t id;
};

static uint32_t baudrate_to_int(hal_uart_baud_rate_t baud)
{
    switch (baud) {
        case HAL_UART_BAUD_RATE_9600: return 9600;
        case HAL_UART_BAUD_RATE_19200: return 19200;
        case HAL_UART_BAUD_RATE_38400: return 38400;
        case HAL_UART_BAUD_RATE_57600: return 57600;
        case HAL_UART_BAUD_RATE_115200: return 115200;
        default: return 9600;
    }
}

static void port_uart_init(void)
{
    for (uint32_t i = 0; i < MAX_UART_PORTS; i++) {
        uart_devices[i] = NULL;
    }
}

static void port_uart_deinit(void)
{
    for (uint32_t i = 0; i < MAX_UART_PORTS; i++) {
        if (uart_devices[i]) {
            hal_uart_close(uart_devices[i]);
            free(uart_devices[i]->port_name);
            free(uart_devices[i]);
            uart_devices[i] = NULL;
        }
    }
}

static void port_uart_open(hal_uart_port_t dev, hal_uart_config_t* cfg)
{
    if (dev >= MAX_UART_PORTS || !cfg)
        return NULL;

    hal_uart_dev_t uart = malloc(sizeof(struct hal_uart_dev_s)); //garantir que o espaço na memória seja diferente

    if (!uart) 
        return NULL;

    char name[16];
    snprintf(name, sizeof(name), "COM%u", dev + 1);
    uart->port_name = strdup(name);
    uart->is_open = false;
    uart->id = dev;
    uart->config = *cfg;

    if (sp_get_port_by_name(uart->port_name, &uart->port) != SP_OK) {
        free(uart->port_name);
        free(uart);
        return NULL;
    }

    if (sp_open(uart->port, SP_MODE_READ_WRITE) != SP_OK) {
        sp_free_port(uart->port);
        free(uart->port_name);
        free(uart);
        return NULL;
    }

    sp_set_baudrate(uart->port, baudrate_to_int(cfg->baud_rate));
    sp_set_bits(uart->port, 8);
    sp_set_stopbits(uart->port, cfg->stop_bits == HAL_UART_STOP_BITS_2 ? 2 : 1);

    enum sp_parity parity = SP_PARITY_NONE;
    if (cfg->parity == HAL_UART_PARITY_ODD) 
        parity = SP_PARITY_ODD;
    else if (cfg->parity == HAL_UART_PARITY_EVEN) 
        parity = SP_PARITY_EVEN;
    sp_set_parity(uart->port, parity);

    sp_set_flowcontrol(uart->port, SP_FLOWCONTROL_NONE); // CTS/RTS não suportado aqui

    uart->is_open = true;
    uart_devices[dev] = uart;

    return uart;
}

static void port_uart_close(hal_uart_dev_t dev)
{
    if (!dev || !dev->is_open) 
        return;
    sp_close(dev->port);
    sp_free_port(dev->port);
    dev->is_open = false;
}

static size_t port_uart_bytes_available(hal_uart_dev_t dev)
{
    if (!dev || !dev->is_open) 
        return 0;
    size_t bytes = 0;
    bytes = (size_t)sp_input_waiting(dev->port);
    return bytes;
}

static ssize_t port_uart_read(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    if (!dev || !dev->is_open || !buffer) 
        return -1;
    int32_t r = sp_blocking_read(dev->port, buffer, size, 1000);
    return r >= 0 ? r : -1;
}

static ssize_t port_uart_write(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    if (!dev || !dev->is_open || !buffer) 
        return -1;
    int32_t r = sp_blocking_write(dev->port, buffer, size, 1000);
    return r >= 0 ? r : -1;
}

static void port_uart_flush(hal_uart_dev_t dev)
{
    if (!dev || !dev->is_open) 
        return;
    sp_flush(dev->port, SP_BUF_BOTH); //limpa buffers de entrada e saída
}

ssize_t hal_uart_byte_read(hal_uart_dev_t dev, uint8_t* c)
{
    return hal_uart_read(dev, c, 1);
}

ssize_t hal_uart_byte_write(hal_uart_dev_t dev, uint8_t c)
{
    return hal_uart_write(dev, &c, 1);
}

hal_uart_driver_t HAL_UART_DRIVER =
{
    .init = port_uart_init,
    .deinit = port_uart_deinit,
    .open = port_uart_open,
    .close = port_uart_close,
    .bytes_available = port_uart_bytes_available,
    .read = port_uart_read,
    .write = port_uart_write,
    .flush = port_uart_flush,
};
