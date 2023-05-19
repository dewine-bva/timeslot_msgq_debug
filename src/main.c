/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/types.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

#include <mpsl_timeslot.h>
#include <mpsl.h>
#include <hal/nrf_timer.h>

#include <zephyr/drivers/gpio.h>
#include "test_gpio.h"

LOG_MODULE_REGISTER(demo, LOG_LEVEL_INF);

/* DEFINES FOR CONFIGURATION*/
// TODO: Should be put in a Kconfig

// If defined create pulses using a workqeue that disables the GPIO
// after some time instead of toggling it.
// (for IDLE and request allowed/denied)
// #define PULSE

// If defined toggle GPIO gpio_test for each message read/send in queue.
// For RX it can be seen how immediate logging increases the duration for reading each element.
// #define GPIO_MSGQ_RX
// #define GPIO_MSGQ_TX

// To test many messages at a faster rate to (probably) more likely get an error.
// Causes lost messages in logging when deffered logging is used.
// #define FAST

// If defined will read less data then put into the queue due to maximum limit, so queue will fill up.
// Not all messages can be sent at some point (should be visible in log -> missing numbers).
#define FILL_BUFFER

// Can be used to not use a message queue but just directly write data in 
// global structure in interrupt that can be read in the other thread.
// In this example the timing guarantees no access at the same time,
// but in general this has not to be the case, an interrupt may change
// the date during reading the values.
// The implmenetation is made to fit the normal operation, not the fast one.
// So, when FAST is used, this will have no effect.
// #define NO_MSGQ

/* END DEFINES FOR CONFIGURATION*/

extern struct gpio_dt_spec gpio_main;
extern struct gpio_dt_spec gpio_qos_req_all;
extern struct gpio_dt_spec gpio_qos_req_den;
extern struct gpio_dt_spec gpio_tim_start;
extern struct gpio_dt_spec gpio_tim_timer;
extern struct gpio_dt_spec gpio_tim_idle;
extern struct gpio_dt_spec gpio_test;

#if defined(FAST)
#define TIMESLOT_LENGTH_US           (1500)
#else
#define TIMESLOT_LENGTH_US           (6700)
#endif

#define TIMER_EXPIRY_US (TIMESLOT_LENGTH_US - 200)

#define MPSL_THREAD_PRIO             CONFIG_MPSL_THREAD_COOP_PRIO
#define STACKSIZE                    CONFIG_MAIN_STACK_SIZE
#define THREAD_PRIORITY              K_LOWEST_APPLICATION_THREAD_PRIO

/* MPSL API calls that can be requested for the non-preemptible thread */
enum mpsl_timeslot_call {
	OPEN_SESSION,
	MAKE_REQUEST,
	CLOSE_SESSION,
};

/* Timeslot requests */
static mpsl_timeslot_request_t timeslot_request_earliest = {
	.request_type = MPSL_TIMESLOT_REQ_TYPE_EARLIEST,
	.params.earliest.hfclk = MPSL_TIMESLOT_HFCLK_CFG_NO_GUARANTEE,
	.params.earliest.priority = MPSL_TIMESLOT_PRIORITY_NORMAL,
	.params.earliest.length_us = TIMESLOT_LENGTH_US,
	.params.earliest.timeout_us = 1000000
};

static mpsl_timeslot_signal_return_param_t signal_callback_return_param;

/* Message queue for requesting MPSL API calls to non-preemptible thread */
K_MSGQ_DEFINE(mpsl_api_msgq, sizeof(enum mpsl_timeslot_call), 10, 4);

// Only allow new request when available
K_SEM_DEFINE(sem_allow_request, 0, 1);

// Send data from ISR to thread
#if defined(NO_MSGQ) && !defined(FAST)
uint8_t data_array[20];
#else
#if defined(FAST)
K_MSGQ_DEFINE(data_msgq, 1, 200, 1);
#else
K_MSGQ_DEFINE(data_msgq, 1, 120, 1);
#endif
#endif


static mpsl_timeslot_signal_return_param_t *mpsl_timeslot_callback(
	mpsl_timeslot_session_id_t session_id,
	uint32_t signal_type)
{
	(void) session_id; /* unused parameter */

	mpsl_timeslot_signal_return_param_t *p_ret_val = NULL;

	switch (signal_type) {

	case MPSL_TIMESLOT_SIGNAL_START:
		/* No return action */
		signal_callback_return_param.callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_NONE;
		p_ret_val = &signal_callback_return_param;

		/* Setup timer to trigger an interrupt (and thus the TIMER0
		 * signal) before timeslot end.
		 */
		nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0, TIMER_EXPIRY_US);
		nrf_timer_int_enable(NRF_TIMER0, NRF_TIMER_INT_COMPARE0_MASK);

		gpio_pin_toggle_dt(&gpio_tim_start);
	
		static uint8_t test_val = 0;
	#if defined(FAST)
		// Even that it is faster want to cause a higer load for this test
		// and also send more messages.
		// With TX pulses activated: Duration is about 500 us for sending all 100 messages.
		for(int count = 0; count < 100; count ++)
	#else
		for(int count = 0; count < 20; count ++)
	#endif
		{
		#if defined(GPIO_MSGQ_TX)
			gpio_pin_toggle_dt(&gpio_test);
		#endif
    		test_val++;
		#if defined(NO_MSGQ) && !defined(FAST)
			data_array[count] = test_val;
		#else

			int err = k_msgq_put(&data_msgq, &test_val, K_NO_WAIT);

			if(err == -ENOMSG)
			{
				// LOG_WRN("ENOMSG");
			}
			else if(err == -EAGAIN)
			{
				// LOG_WRN("EAGAIN");
			}
		#endif
		}

		break;
	case MPSL_TIMESLOT_SIGNAL_TIMER0:
		/* Clear event */
		nrf_timer_int_disable(NRF_TIMER0, NRF_TIMER_INT_COMPARE0_MASK);
		nrf_timer_event_clear(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0);

		gpio_pin_toggle_dt(&gpio_tim_timer);

		signal_callback_return_param.callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_END;
		p_ret_val = &signal_callback_return_param;
		
		break;
	case MPSL_TIMESLOT_SIGNAL_SESSION_IDLE:
	#if defined(PULSE)
		gpio_pin_set_dt(&gpio_tim_idle, true);
		disable_gpio_work(gpio_tim_idle.pin);
	#else
		gpio_pin_toggle_dt(&gpio_tim_idle);
	#endif

		k_sem_give(&sem_allow_request);

		break;
	case MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED:
		// Will not happen in this example

		break;
	default:
		LOG_ERR("unexpected signal: %u", signal_type);
		k_oops();
		break;
	}

	return p_ret_val;
}

static void mpsl_timeslot_demo(void)
{
	int err;
	enum mpsl_timeslot_call api_call;

	api_call = OPEN_SESSION;
	err = k_msgq_put(&mpsl_api_msgq, &api_call, K_FOREVER);
	if (err) {
		LOG_ERR("Message sent error: %d", err);
		k_oops();
	}

	while(1)
	{
		api_call = MAKE_REQUEST;

		int sem_code = k_sem_take(&sem_allow_request, K_NO_WAIT);

		if(sem_code == 0)
		{
		#if defined(PULSE)
			gpio_pin_set_dt(&gpio_qos_req_all, true);
			disable_gpio_work(gpio_qos_req_all.pin);
		#else
			gpio_pin_toggle_dt(&gpio_qos_req_all);
		#endif

			err = k_msgq_put(&mpsl_api_msgq, &api_call, K_FOREVER);
			if (err) {
				LOG_ERR("Message sent error: %d", err);
				k_oops();
			}
		}
		else
		{
			#if defined(PULSE)
				gpio_pin_set_dt(&gpio_qos_req_den, true);
				disable_gpio_work(gpio_qos_req_den.pin);
			#else
				gpio_pin_toggle_dt(&gpio_qos_req_den);
			#endif
		}

	#if defined(FAST)
		k_msleep(12);
	#else
		k_msleep(100);
	#endif

#if defined(NO_MSGQ) && !defined(FAST)
	LOG_INF("START");
	for(int i=0; i<20; i++)
	{
		LOG_INF("%2d: %3d", i, data_array[i]);
	}
	// LOG_INF("%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d",
	// 	data_array[0], data_array[1], data_array[2], data_array[3], data_array[4],
	// 	data_array[5], data_array[6], data_array[7], data_array[8], data_array[9],
	// 	data_array[10], data_array[11], data_array[12], data_array[13], data_array[14],
	// 	data_array[15], data_array[16], data_array[17], data_array[18], data_array[19]
	// 	);
#else
		uint8_t data_recv;

		uint8_t max_data_count = 0;
	
		do
		{
			if(k_msgq_get(&data_msgq, &data_recv, K_NO_WAIT) != 0)
			{
				break;
			}

		#if defined(GPIO_MSGQ_RX)
			gpio_pin_toggle_dt(&gpio_test);
		#endif
	
			LOG_INF("%d", data_recv);

			max_data_count++;

	// In fast mode always sending 100 messages, in normal mode 20
	#if defined(FAST)
		#if defined(FILL_BUFFER)
			if(max_data_count == 98)
		#else
			if(max_data_count == 120)
		#endif
	#else
		#if defined(FILL_BUFFER)
			if(max_data_count == 18)
		#else
			if(max_data_count == 40)
		#endif
	#endif
			{
				LOG_INF("STOP MSG");
				break;
			}

		}while(1);

		LOG_INF("NO MORE MSG");

#endif

	}
}

/* To ensure thread safe operation, call all MPSL APIs from a non-preemptible
 * thread.
 */
static void mpsl_nonpreemptible_thread(void)
{
	int err;
	enum mpsl_timeslot_call api_call = 0;

	/* Initialize to invalid session id */
	mpsl_timeslot_session_id_t session_id = 0xFFu;

	while (1) {
		if (k_msgq_get(&mpsl_api_msgq, &api_call, K_FOREVER) == 0) {
			switch (api_call) {
			case OPEN_SESSION:
				err = mpsl_timeslot_session_open(
					mpsl_timeslot_callback,
					&session_id);
				if (err) {
					LOG_ERR("Timeslot session open error: %d", err);
					k_oops();
				}
				LOG_INF("Opened session");
				k_sem_give(&sem_allow_request);
				break;
			case MAKE_REQUEST:
				err = mpsl_timeslot_request(
					session_id,
					&timeslot_request_earliest);
				if (err) {
					LOG_ERR("Timeslot request error: %d", err);
					k_oops();
				}
				break;
			case CLOSE_SESSION:
				err = mpsl_timeslot_session_close(session_id);
				if (err) {
					LOG_ERR("Timeslot session close error: %d", err);
					k_oops();
				}
				LOG_INF("Closed session");
				break;
			default:
				LOG_ERR("Wrong timeslot API call");
				k_oops();
				break;
			}
		}
	}
}

void main(void)
{
	int err = init_test_gpios();
	if(err < 0)
	{
		LOG_ERR("GPIO INIT ERROR");
		return;
	}

	while(1)
	{
		gpio_pin_toggle_dt(&gpio_main);

		k_sleep(K_MSEC(15));
	}
}

K_THREAD_DEFINE(mpsl_timeslot_demo_thread_id, STACKSIZE,
		mpsl_timeslot_demo, NULL, NULL, NULL,
		5, 0, 0);

K_THREAD_DEFINE(mpsl_nonpreemptible_thread_id, STACKSIZE,
		mpsl_nonpreemptible_thread, NULL, NULL, NULL,
		K_PRIO_COOP(MPSL_THREAD_PRIO), 0, 0);