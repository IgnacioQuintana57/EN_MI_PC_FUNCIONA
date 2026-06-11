#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/bitops.h>

#define DEVICE_NAME "tp5_signal"
#define CLASS_NAME  "tp5_class"

/*
 * Raspberry Pi 400 / Raspberry Pi 4 usan BCM2711.
 * Base física del controlador GPIO en BCM2711:
 */
#define GPIO_BASE_PHYS 0xFE200000
#define GPIO_SIZE      0x100

/*
 * Offsets de registros GPIO.
 */
#define GPFSEL0_OFFSET 0x00
#define GPLEV0_OFFSET  0x34

/*
 * Señales externas:
 *
 * Señal 0: Arduino D8 -> divisor resistivo -> Raspberry GPIO17
 * Señal 1: Arduino D9 -> divisor resistivo -> Raspberry GPIO27
 */
#define GPIO_SIGNAL_0 17
#define GPIO_SIGNAL_1 27

/*
 * La consigna pide sensar con período de 1 segundo.
 *
 * Para pruebas visuales, si querés ver más movimiento en el gráfico,
 * podés bajar temporalmente esto a 200 ms.
 */
#define SAMPLE_PERIOD_MS 100

static dev_t first;
static struct cdev c_dev;
static struct class *cl;

static struct timer_list sample_timer;

static void __iomem *gpio_base;

static int selected_signal = 0;
static int last_signal_0 = 0;
static int last_signal_1 = 0;
static u64 last_sample_ms = 0;

static inline void __iomem *gpio_reg(u32 offset)
{
    return (void __iomem *)((u8 __iomem *)gpio_base + offset);
}

/*
 * Configura un GPIO como entrada.
 *
 * En los registros GPFSEL:
 * cada GPIO usa 3 bits.
 * 000 = input
 */
static void tp5_gpio_set_input(unsigned int gpio)
{
    unsigned int reg_offset;
    unsigned int shift;
    u32 value;

    reg_offset = GPFSEL0_OFFSET + ((gpio / 10) * 4);
    shift = (gpio % 10) * 3;

    value = ioread32(gpio_reg(reg_offset));
    value &= ~(0x7 << shift);
    iowrite32(value, gpio_reg(reg_offset));
}

/*
 * Lee el nivel lógico del GPIO.
 *
 * Para GPIO17 y GPIO27 alcanza con GPLEV0,
 * porque ambos están entre GPIO0 y GPIO31.
 */
static int tp5_gpio_read(unsigned int gpio)
{
    u32 level;

    level = ioread32(gpio_reg(GPLEV0_OFFSET));
    return !!(level & BIT(gpio));
}

/*
 * Timer periódico: toma una muestra de ambas señales cada SAMPLE_PERIOD_MS.
 */
static void sample_timer_callback(struct timer_list *timer)
{
    last_signal_0 = tp5_gpio_read(GPIO_SIGNAL_0);
    last_signal_1 = tp5_gpio_read(GPIO_SIGNAL_1);
    last_sample_ms = ktime_to_ms(ktime_get_boottime());

    mod_timer(&sample_timer, jiffies + msecs_to_jiffies(SAMPLE_PERIOD_MS));
}

static int my_open(struct inode *i, struct file *f)
{
    printk(KERN_INFO "tp5_signal: open()\n");
    return 0;
}

static int my_close(struct inode *i, struct file *f)
{
    printk(KERN_INFO "tp5_signal: close()\n");
    return 0;
}

/*
 * read() devuelve una línea CSV:
 *
 * signal,value,timestamp_ms
 *
 * Ejemplo:
 * 0,1,123456
 */
static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[64];
    int value;
    int n;

    if (selected_signal == 0)
        value = last_signal_0;
    else
        value = last_signal_1;

    n = scnprintf(
        kbuf,
        sizeof(kbuf),
        "%d,%d,%llu\n",
        selected_signal,
        value,
        (unsigned long long)last_sample_ms
    );

    return simple_read_from_buffer(buf, len, off, kbuf, n);
}

/*
 * write() permite seleccionar qué señal leer:
 *
 * echo 0 > /dev/tp5_signal
 * echo 1 > /dev/tp5_signal
 */
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    char kbuf[8];

    if (len == 0)
        return -EINVAL;

    if (len >= sizeof(kbuf))
        len = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, buf, len) != 0)
        return -EFAULT;

    kbuf[len] = '\0';

    if (kbuf[0] == '0') {
        selected_signal = 0;
        printk(KERN_INFO "tp5_signal: seleccionada señal 0 GPIO%d\n", GPIO_SIGNAL_0);
    } else if (kbuf[0] == '1') {
        selected_signal = 1;
        printk(KERN_INFO "tp5_signal: seleccionada señal 1 GPIO%d\n", GPIO_SIGNAL_1);
    } else {
        printk(KERN_WARNING "tp5_signal: valor inválido. Use 0 o 1.\n");
        return -EINVAL;
    }

    return len;
}

static struct file_operations tp5_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
    .read = my_read,
    .write = my_write
};

static int __init tp5_init(void)
{
    int ret;
    struct device *dev_ret;

    printk(KERN_INFO "tp5_signal: iniciando driver\n");

    /*
     * Mapeamos el bloque GPIO físico del BCM2711.
     */
    gpio_base = ioremap(GPIO_BASE_PHYS, GPIO_SIZE);
    if (!gpio_base) {
        printk(KERN_ERR "tp5_signal: no se pudo mapear GPIO_BASE 0x%X\n", GPIO_BASE_PHYS);
        return -ENOMEM;
    }

    /*
     * Configuramos los GPIO como entradas.
     */
    tp5_gpio_set_input(GPIO_SIGNAL_0);
    tp5_gpio_set_input(GPIO_SIGNAL_1);

    printk(KERN_INFO "tp5_signal: GPIO%d y GPIO%d configurados como entrada por MMIO\n",
           GPIO_SIGNAL_0, GPIO_SIGNAL_1);

    /*
     * Primera lectura inicial.
     */
    last_signal_0 = tp5_gpio_read(GPIO_SIGNAL_0);
    last_signal_1 = tp5_gpio_read(GPIO_SIGNAL_1);
    last_sample_ms = ktime_to_ms(ktime_get_boottime());

    /*
     * Inicializamos el timer periódico.
     */
    timer_setup(&sample_timer, sample_timer_callback, 0);
    mod_timer(&sample_timer, jiffies + msecs_to_jiffies(SAMPLE_PERIOD_MS));

    /*
     * Registramos el character device.
     */
    ret = alloc_chrdev_region(&first, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "tp5_signal: error en alloc_chrdev_region\n");
        timer_shutdown_sync(&sample_timer);
        iounmap(gpio_base);
        return ret;
    }

    cdev_init(&c_dev, &tp5_fops);

    ret = cdev_add(&c_dev, first, 1);
    if (ret < 0) {
        printk(KERN_ERR "tp5_signal: error en cdev_add\n");
        unregister_chrdev_region(first, 1);
        timer_shutdown_sync(&sample_timer);
        iounmap(gpio_base);
        return ret;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cl = class_create(CLASS_NAME);
#else
    cl = class_create(THIS_MODULE, CLASS_NAME);
#endif

    if (IS_ERR(cl)) {
        printk(KERN_ERR "tp5_signal: error en class_create\n");
        cdev_del(&c_dev);
        unregister_chrdev_region(first, 1);
        timer_shutdown_sync(&sample_timer);
        iounmap(gpio_base);
        return PTR_ERR(cl);
    }

    dev_ret = device_create(cl, NULL, first, NULL, DEVICE_NAME);
    if (IS_ERR(dev_ret)) {
        printk(KERN_ERR "tp5_signal: error en device_create\n");
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(first, 1);
        timer_shutdown_sync(&sample_timer);
        iounmap(gpio_base);
        return PTR_ERR(dev_ret);
    }

    printk(KERN_INFO "tp5_signal: driver cargado correctamente\n");
    printk(KERN_INFO "tp5_signal: device creado en /dev/%s\n", DEVICE_NAME);
    printk(KERN_INFO "tp5_signal: señal 0 GPIO%d, señal 1 GPIO%d\n",
           GPIO_SIGNAL_0, GPIO_SIGNAL_1);

    return 0;
}

static void __exit tp5_exit(void)
{
    timer_shutdown_sync(&sample_timer);

    device_destroy(cl, first);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(first, 1);

    if (gpio_base)
        iounmap(gpio_base);

    printk(KERN_INFO "tp5_signal: driver descargado\n");
}

module_init(tp5_init);
module_exit(tp5_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sistemas de Computacion - TP5");
MODULE_DESCRIPTION("CDD TP5: lectura de dos señales externas por GPIO usando MMIO");