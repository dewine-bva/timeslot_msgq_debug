#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/types.h>

#include "test_gpio.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(gpio, LOG_LEVEL_INF);

#define GPIO_DELAY_US 100

// get device information from device tree
struct gpio_dt_spec gpio_main = GPIO_DT_SPEC_GET(DT_NODELABEL(main), gpios);
struct gpio_dt_spec gpio_qos_req_all = GPIO_DT_SPEC_GET(DT_NODELABEL(qos_req_all), gpios);
struct gpio_dt_spec gpio_qos_req_den = GPIO_DT_SPEC_GET(DT_NODELABEL(qos_req_den), gpios);
struct gpio_dt_spec gpio_tim_start = GPIO_DT_SPEC_GET(DT_NODELABEL(tim_start), gpios);
struct gpio_dt_spec gpio_tim_timer = GPIO_DT_SPEC_GET(DT_NODELABEL(tim_timer), gpios);
struct gpio_dt_spec gpio_tim_radio = GPIO_DT_SPEC_GET(DT_NODELABEL(tim_radio), gpios);
struct gpio_dt_spec gpio_tim_block = GPIO_DT_SPEC_GET(DT_NODELABEL(tim_block), gpios);
struct gpio_dt_spec gpio_tim_cancel = GPIO_DT_SPEC_GET(DT_NODELABEL(tim_cancel), gpios);
struct gpio_dt_spec gpio_tim_idle = GPIO_DT_SPEC_GET(DT_NODELABEL(tim_idle), gpios);

struct gpio_dt_spec gpio_test = GPIO_DT_SPEC_GET(DT_NODELABEL(test), gpios);

// Define function for each pin to clear the GPIOs.
// For each pin a separate function, as it will be called via a work queue and
// a work queue does not allow parameters for the function called.
static void disable_gpio_main() 
{
    gpio_pin_set_dt(&gpio_main, false);
}

static void disable_gpio_qos_req_all() 
{
    gpio_pin_set_dt(&gpio_qos_req_all, false);
}

static void disable_gpio_qos_req_den() 
{
    gpio_pin_set_dt(&gpio_qos_req_den, false);
}

static void disable_gpio_tim_start() 
{
    gpio_pin_set_dt(&gpio_tim_start, false);
}

static void disable_gpio_tim_timer() 
{
    gpio_pin_set_dt(&gpio_tim_timer, false);
}

static void disable_gpio_tim_radio() 
{
    gpio_pin_set_dt(&gpio_tim_radio, false);
}

static void disable_gpio_tim_block() 
{
    gpio_pin_set_dt(&gpio_tim_block, false);
}

static void disable_gpio_tim_cancel() 
{
    gpio_pin_set_dt(&gpio_tim_cancel, false);
}

static void disable_gpio_tim_idle() 
{
    gpio_pin_set_dt(&gpio_tim_idle, false);
}


static void disable_gpio_test() 
{
    gpio_pin_set_dt(&gpio_test, false);
}


// Define delayabale work items for all functions to clear GPIOs.
static K_WORK_DELAYABLE_DEFINE(gpio_main_work, disable_gpio_main); 
static K_WORK_DELAYABLE_DEFINE(gpio_qos_req_all_work, disable_gpio_qos_req_all);
static K_WORK_DELAYABLE_DEFINE(gpio_qos_req_den_work, disable_gpio_qos_req_den);
static K_WORK_DELAYABLE_DEFINE(gpio_tim_start_work, disable_gpio_tim_start);
static K_WORK_DELAYABLE_DEFINE(gpio_tim_timer_work, disable_gpio_tim_timer);
static K_WORK_DELAYABLE_DEFINE(gpio_tim_radio_work, disable_gpio_tim_radio);
static K_WORK_DELAYABLE_DEFINE(gpio_tim_block_work, disable_gpio_tim_block);
static K_WORK_DELAYABLE_DEFINE(gpio_tim_cancel_work, disable_gpio_tim_cancel);
static K_WORK_DELAYABLE_DEFINE(gpio_tim_idle_work, disable_gpio_tim_idle);

static K_WORK_DELAYABLE_DEFINE(gpio_test_work, disable_gpio_test);

// Function used to trigger the work queue scheduling from outside.
// Pin of GPIO to trigger needs to be provided.
void disable_gpio_work(gpio_pin_t pin)
{
    struct k_work_delayable *work = NULL;

    if(pin == gpio_main.pin)
    {
        work = &gpio_main_work;
    }
    else if(pin == gpio_qos_req_all.pin)
    {
        work = &gpio_qos_req_all_work;
    }
    else if(pin == gpio_qos_req_den.pin)
    {
        work = &gpio_qos_req_den_work;
    }
    else if(pin == gpio_tim_start.pin)
    {
        work = &gpio_tim_start_work;
    }
    else if(pin == gpio_tim_timer.pin)
    {
        work = &gpio_tim_timer_work;
    }
    else if(pin == gpio_tim_radio.pin)
    {
        work = &gpio_tim_radio_work;
    }
    else if(pin == gpio_tim_block.pin)
    {
        work = &gpio_tim_block_work;
    }
    else if(pin == gpio_tim_cancel.pin)
    {
        work = &gpio_tim_cancel_work;
    }
    else if(pin == gpio_tim_idle.pin)
    {
        work = &gpio_tim_idle_work;
    }
    else if(pin == gpio_test.pin)
    {
        work = &gpio_test_work;
    }
    else
    {
        LOG_WRN("Invalid GPIO pin number to trigger setting GPIO to low!");
        return;
    }
    k_work_schedule(work, K_USEC(GPIO_DELAY_US));
}

// Init all GPIOs directly from one call
int init_test_gpios()
{
    // TODO: differentiate for which gpio error occured. 
    // Currently rest of GPIOs would not be initialized when an error occurs.
    int err;
    err = gpio_pin_configure_dt(&gpio_main, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err;}
    err = gpio_pin_configure_dt(&gpio_qos_req_all, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_qos_req_den, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_tim_start, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_tim_timer, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_tim_radio, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_tim_block, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_tim_cancel, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }
    err = gpio_pin_configure_dt(&gpio_tim_idle, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }

    err = gpio_pin_configure_dt(&gpio_test, GPIO_OUTPUT_INACTIVE);
    if(err < 0) { return err; }

    return 0;
}