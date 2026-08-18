/*
 * SPDX-License-Identifier: Apache-2.0
 */

struct native_gpio_pin {
    const struct device *port;
    gpio_pin_t pin;
};

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio0), okay)
#define NATIVE_GPIO_PORT0 DT_NODELABEL(gpio0)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(gpioa), okay)
#define NATIVE_GPIO_PORT0 DT_NODELABEL(gpioa)
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio1), okay)
#define NATIVE_GPIO_PORT1 DT_NODELABEL(gpio1)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(gpiob), okay)
#define NATIVE_GPIO_PORT1 DT_NODELABEL(gpiob)
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio2), okay)
#define NATIVE_GPIO_PORT2 DT_NODELABEL(gpio2)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(gpioc), okay)
#define NATIVE_GPIO_PORT2 DT_NODELABEL(gpioc)
#endif

#ifdef NATIVE_GPIO_PORT0
#define NATIVE_GPIO_PIN_PORT0(pin_) \
    { .port = DEVICE_DT_GET(NATIVE_GPIO_PORT0), .pin = (pin_) }
#else
#define NATIVE_GPIO_PIN_PORT0(pin_) \
    { .port = NULL, .pin = 0 }
#endif

#ifdef NATIVE_GPIO_PORT1
#define NATIVE_GPIO_PIN_PORT1(pin_) \
    { .port = DEVICE_DT_GET(NATIVE_GPIO_PORT1), .pin = (pin_) }
#else
#define NATIVE_GPIO_PIN_PORT1(pin_) \
    { .port = NULL, .pin = 0 }
#endif

#ifdef NATIVE_GPIO_PORT2
#define NATIVE_GPIO_PIN_PORT2(pin_) \
    { .port = DEVICE_DT_GET(NATIVE_GPIO_PORT2), .pin = (pin_) }
#else
#define NATIVE_GPIO_PIN_PORT2(pin_) \
    { .port = NULL, .pin = 0 }
#endif

/*
 * Arduino pin numbers are application ABI identifiers. Keep SoC/carrier-board
 * mapping here so native applications never depend on Zephyr device pointers.
 */
static const struct native_gpio_pin native_gpio_pin_map[] = {
    NATIVE_GPIO_PIN_PORT0(0),  NATIVE_GPIO_PIN_PORT0(1),
    NATIVE_GPIO_PIN_PORT0(2),  NATIVE_GPIO_PIN_PORT0(3),
    NATIVE_GPIO_PIN_PORT0(4),  NATIVE_GPIO_PIN_PORT0(5),
    NATIVE_GPIO_PIN_PORT0(6),  NATIVE_GPIO_PIN_PORT0(7),
    NATIVE_GPIO_PIN_PORT0(8),  NATIVE_GPIO_PIN_PORT0(9),
    NATIVE_GPIO_PIN_PORT0(10), NATIVE_GPIO_PIN_PORT0(11),
    NATIVE_GPIO_PIN_PORT0(12), NATIVE_GPIO_PIN_PORT0(13),
    NATIVE_GPIO_PIN_PORT0(14), NATIVE_GPIO_PIN_PORT0(15),
    NATIVE_GPIO_PIN_PORT1(0),  NATIVE_GPIO_PIN_PORT1(1),
    NATIVE_GPIO_PIN_PORT1(2),  NATIVE_GPIO_PIN_PORT1(3),
    NATIVE_GPIO_PIN_PORT1(4),  NATIVE_GPIO_PIN_PORT1(5),
    NATIVE_GPIO_PIN_PORT1(6),  NATIVE_GPIO_PIN_PORT1(7),
    NATIVE_GPIO_PIN_PORT1(8),  NATIVE_GPIO_PIN_PORT1(9),
    NATIVE_GPIO_PIN_PORT1(10), NATIVE_GPIO_PIN_PORT1(11),
    NATIVE_GPIO_PIN_PORT1(12), NATIVE_GPIO_PIN_PORT1(13),
    NATIVE_GPIO_PIN_PORT1(14), NATIVE_GPIO_PIN_PORT1(15),
    NATIVE_GPIO_PIN_PORT2(0),  NATIVE_GPIO_PIN_PORT2(1),
    NATIVE_GPIO_PIN_PORT2(2),  NATIVE_GPIO_PIN_PORT2(3),
    NATIVE_GPIO_PIN_PORT2(4),  NATIVE_GPIO_PIN_PORT2(5),
    NATIVE_GPIO_PIN_PORT2(6),  NATIVE_GPIO_PIN_PORT2(7),
    NATIVE_GPIO_PIN_PORT2(8),  NATIVE_GPIO_PIN_PORT2(9),
    NATIVE_GPIO_PIN_PORT2(10), NATIVE_GPIO_PIN_PORT2(11),
    NATIVE_GPIO_PIN_PORT2(12), NATIVE_GPIO_PIN_PORT2(13),
    NATIVE_GPIO_PIN_PORT2(14), NATIVE_GPIO_PIN_PORT2(15),
};

static int native_api_rtbus_post(uint8_t task_id, uint32_t ctx_id,
                                 const void *payload, uint32_t payload_len)
{
    struct native_rtbus_post_msg msg = {
        .task_id = task_id,
        .ctx_id = ctx_id,
        .payload_len = payload_len,
    };

    int ret;

    /*
     * Keep the source payload valid until it has been copied into the runtime
     * queue. The upgrade handoff checks this counter before force-aborting the
     * native application thread.
     */
    atomic_inc(&native_api_busy);

    if (payload_len > NATIVE_RTBUS_POST_PAYLOAD_MAX) {
        ret = -EMSGSIZE;
        goto out;
    }

    if (payload_len != 0 && payload == NULL) {
        ret = -EINVAL;
        goto out;
    }

    if (payload_len != 0) {
        memcpy(msg.payload, payload, payload_len);
    }

    ret = k_msgq_put(&native_rtbus_post_msgq, &msg, K_NO_WAIT);

    if (ret == 0) {
        /* Wake the runtime loop; EMOS will drain this msgq from its context. */
        runtime_event_signal();
    }

out:
    atomic_dec(&native_api_busy);
    return ret;
}

static int native_api_result_wait(uint32_t timeout_ms)
{
    if (timeout_ms == WZ_WAIT_FOREVER) {
        return k_sem_take(&native_result_sem, K_FOREVER);
    }

    return k_sem_take(&native_result_sem, K_MSEC(timeout_ms));
}

static int native_api_on_event(runtime_event_callback_t callback)
{
    native_event_callback = callback;
    return 0;
}

static int32_t native_api_k_delay(uint32_t delay_ms)
{
    int64_t deadline = k_uptime_get() + delay_ms;

    native_service_dispatch_events();

    while (true) {
        int64_t remaining_ms = deadline - k_uptime_get();
        uint32_t events;

        if (atomic_get(&native_patch_handoff) != 0) {
            return -EINTR;
        }

        if (remaining_ms <= 0) {
            break;
        }

        events = k_event_wait(&native_wake_event,
                              NATIVE_WAKE_ALL,
                              true,
                              K_MSEC(remaining_ms));
        if ((events & NATIVE_WAKE_IRQ) != 0U) {
            native_service_dispatch_events();
        }
    }

    native_service_dispatch_events();
    return 0;
}

static int native_gpio_resolve(uint32_t pin,
                               const struct native_gpio_pin **entry)
{
    if (pin >= ARRAY_SIZE(native_gpio_pin_map)) {
        return -EINVAL;
    }

    *entry = &native_gpio_pin_map[pin];
    if ((*entry)->port == NULL || !device_is_ready((*entry)->port)) {
        return -ENODEV;
    }

    return 0;
}

static int native_api_gpio_configure(uint32_t pin, uint32_t mode)
{
    const struct native_gpio_pin *entry;
    gpio_flags_t flags;
    int ret;

    ret = native_gpio_resolve(pin, &entry);
    if (ret != 0) {
        return ret;
    }

    switch (mode) {
    case RUNTIME_GPIO_MODE_INPUT:
        flags = GPIO_INPUT;
        break;
    case RUNTIME_GPIO_MODE_OUTPUT:
        flags = GPIO_OUTPUT;
        break;
    case RUNTIME_GPIO_MODE_INPUT_PULLUP:
        flags = GPIO_INPUT | GPIO_PULL_UP;
        break;
    case RUNTIME_GPIO_MODE_INPUT_PULLDOWN:
        flags = GPIO_INPUT | GPIO_PULL_DOWN;
        break;
    default:
        return -EINVAL;
    }

    return gpio_pin_configure(entry->port, entry->pin, flags);
}

static int native_api_gpio_write(uint32_t pin, uint32_t value)
{
    const struct native_gpio_pin *entry;
    int ret;

    ret = native_gpio_resolve(pin, &entry);
    if (ret != 0) {
        return ret;
    }

    return gpio_pin_set(entry->port, entry->pin, value != 0U);
}

static int native_api_gpio_read(uint32_t pin)
{
    const struct native_gpio_pin *entry;
    int ret;

    ret = native_gpio_resolve(pin, &entry);
    if (ret != 0) {
        return ret;
    }

    return gpio_pin_get(entry->port, entry->pin);
}

static void native_api_printk(const char *fmt, ...)
{
    va_list args;

    if (atomic_get(&native_console_suppressed) != 0) {
        return;
    }

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

static void native_api_vprintk(const char *fmt, va_list args)
{
    if (atomic_get(&native_console_suppressed) != 0) {
        return;
    }

    vprintk(fmt, args);
}

static size_t native_api_serial_write(const uint8_t *data, size_t size)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    size_t written = 0U;

    if (data == NULL || size == 0U || !device_is_ready(dev) ||
        atomic_get(&native_console_suppressed) != 0) {
        return 0U;
    }

    /*
     * The source buffer may live in native application RAM. Keep upgrade
     * handoff from clearing shared RAM while runtime is draining it.
     */
    atomic_inc(&native_api_busy);

    for (size_t i = 0U; i < size; i++) {
        uart_poll_out(dev, data[i]);
        written++;
    }

    atomic_dec(&native_api_busy);
    return written;
}

/* Strong override for mod_rtbus's weak async-post hook. */
__strong int rtbus_process_async_posts(void)
{
    struct native_rtbus_post_msg msg;

    while (k_msgq_get(&native_rtbus_post_msgq, &msg, K_NO_WAIT) == 0) {
        int ret;

        /*
         * Rtbus execution happens in runtime context. Mark it busy so
         * upgrade handoff waits instead of aborting the producer while this
         * request is being consumed.
         */
        atomic_inc(&native_api_busy);
        ret = rtbus_post(msg.task_id,
                            msg.ctx_id,
                            msg.payload_len == 0U ? NULL : msg.payload,
                            msg.payload_len);
        atomic_dec(&native_api_busy);

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}
