#include <asm/current.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
// #include <linux/kdev_t>

#define DEVICE_NAME "ChrDev_XBA"
#define DEVICE_BUFFER_SIZE 32
#define DEVICE_BUFFER_SIZE_FORMAT_LEN 2
#define BLOCK_STR(file) (file->f_flags & O_NONBLOCK ? "BLK-" : "BLK+")
#define IS_NONBLOCK(file) (file->f_flags & O_NONBLOCK)
#define DEVICE_INFO(file) DEVICE_NAME, current->pid, BLOCK_STR(file)
#define BUFFER_INFO(buffer) DEVICE_BUFFER_SIZE_FORMAT_LEN, kfifo_len(&buffer), \
                            DEVICE_BUFFER_SIZE_FORMAT_LEN, DEVICE_BUFFER_SIZE
#define GENERAL_INFO(file, buffer) DEVICE_INFO(file), BUFFER_INFO(buffer)
#define DEVICE_INFO_FMT "[%s::%d::%s]: "
#define BUFFER_INFO_FMT "(%*d/%*d) "
#define GENERAL_INFO_FMT DEVICE_INFO_FMT BUFFER_INFO_FMT

MODULE_LICENSE("GPL");

static int chr_open(struct inode* inode, struct file* filp);
static int chr_release(struct inode* inode, struct file* filp);
static ssize_t chr_read(struct file* file, char __user* buf, size_t lbuf, loff_t* ppos);
static ssize_t chr_write(struct file* file, const char __user* buf, size_t count, loff_t* f_pos);

struct kfifo buffer;
struct mutex mutex_read, mutex_write;
wait_queue_head_t wq_read;
wait_queue_head_t wq_write;

static dev_t dev_no;
static struct cdev chr_dev;
static struct class* dev_class;
static const struct file_operations chr_fops = {
    .open = chr_open,
    .read = chr_read,
    .write = chr_write,
    .release = chr_release,
};

static int chr_open(struct inode* inode, struct file* filp) {
    printk(KERN_INFO DEVICE_INFO_FMT "进程 %d 打开设备.\n", DEVICE_INFO(filp), current->pid);
    return 0;
}

static int chr_release(struct inode* inode, struct file* filp) {
    printk(KERN_INFO DEVICE_INFO_FMT "进程 %d 释放设备.\n", DEVICE_INFO(filp), current->pid);
    return 0;
}

static ssize_t chr_read(struct file* file, char __user* buf, size_t lbuf, loff_t* ppos) {
    int copiedLen = 0;
    int ret = 0;
    char tempBuf[DEVICE_BUFFER_SIZE + 1] = "\0";

    /* 开始处理读请求 */
    mutex_lock(&mutex_read);
    tempBuf[kfifo_out_peek(&buffer, tempBuf, DEVICE_BUFFER_SIZE)] = '\0';
    printk(KERN_INFO GENERAL_INFO_FMT "读取 %2ld 字节 ...\t\t[%s]\n", GENERAL_INFO(file, buffer), lbuf, tempBuf);

    /* 读请求的长度大于缓冲区总长度 */
    if (lbuf > DEVICE_BUFFER_SIZE) {
        printk(KERN_INFO GENERAL_INFO_FMT "读取 %2ld 字节 失败, 请求过长.\n", GENERAL_INFO(file, buffer), lbuf);
        mutex_unlock(&mutex_read);
        return -EFAULT;
    }

    /* 缓冲区中的数据长度小于读请求的长度 */
    if (kfifo_len(&buffer) < lbuf) {
        if (IS_NONBLOCK(file)) {
            printk(KERN_INFO GENERAL_INFO_FMT "读取 %2ld 字节 失败: 请求过长.\n", GENERAL_INFO(file, buffer), lbuf);
            mutex_unlock(&mutex_read);
            return -EAGAIN;
        } else {
            printk(KERN_INFO GENERAL_INFO_FMT "读取 %2ld 字节 等待数据 ...\n", GENERAL_INFO(file, buffer), lbuf);
            ret = wait_event_interruptible(wq_read, kfifo_len(&buffer) >= lbuf);
            if (ret == -ERESTARTSYS) {
                printk(KERN_INFO GENERAL_INFO_FMT "读取 %2ld 字节 失败: 意外中断.\n", GENERAL_INFO(file, buffer), lbuf);
                mutex_unlock(&mutex_read);
                return -EAGAIN;
            }
        }
    }

    /* 读取数据 */
    tempBuf[kfifo_out_peek(&buffer, tempBuf, lbuf)] = '\0';
    ret = kfifo_to_user(&buffer, buf, lbuf, &copiedLen);

    /* 唤醒其他进程 */
    wake_up_interruptible(&wq_write);
    printk(KERN_INFO GENERAL_INFO_FMT "读取 %2ld 字节 成功.\t\t-[%s]\n", GENERAL_INFO(file, buffer), lbuf, tempBuf);
    mutex_unlock(&mutex_read);

    return copiedLen;
}

static ssize_t chr_write(struct file* file, const char __user* buf, size_t count, loff_t* f_pos) {
    int copiedLen = 0;
    int ret = 0;
    char tempBuf[DEVICE_BUFFER_SIZE + 1] = "\0";

    /* 开始处理写请求 */
    mutex_lock(&mutex_write);
    tempBuf[kfifo_out_peek(&buffer, tempBuf, DEVICE_BUFFER_SIZE)] = '\0';
    printk(KERN_INFO GENERAL_INFO_FMT "写入 %2ld 字节 ...\t\t[%s]\n", GENERAL_INFO(file, buffer), count, tempBuf);

    /* 读请求的长度大于缓冲区总长度 */
    if (count > DEVICE_BUFFER_SIZE) {
        printk(KERN_INFO GENERAL_INFO_FMT "写入 %2ld 字节 失败: 请求过长.\n", GENERAL_INFO(file, buffer), count);
        mutex_unlock(&mutex_write);
        return -EFAULT;
    }

    /* 缓冲区中的空位长度小于写请求的长度 */
    if (kfifo_avail(&buffer) < count) {
        if (IS_NONBLOCK(file)) {
            printk(KERN_INFO GENERAL_INFO_FMT "写入 %2ld 字节 失败: 请求过长.\n", GENERAL_INFO(file, buffer), count);
            mutex_unlock(&mutex_write);
            return -EAGAIN;
        } else {
            printk(KERN_INFO GENERAL_INFO_FMT "写入 %2ld 字节 等待数据 ...\n", GENERAL_INFO(file, buffer), count);
            ret = wait_event_interruptible(wq_write, kfifo_avail(&buffer) >= count);
            if (ret == -ERESTARTSYS) {
                printk(KERN_INFO GENERAL_INFO_FMT "写入 %2ld 字节 失败: 意外中断.\n", GENERAL_INFO(file, buffer), count);
                mutex_unlock(&mutex_write);
                return -EAGAIN;
            }
        }
    }

    /* 写入数据 */
    ret = kfifo_from_user(&buffer, buf, count, &copiedLen);

    /* 唤醒其他进程 */
    wake_up_interruptible(&wq_read);
    ret = copy_from_user(tempBuf, buf, copiedLen);
    tempBuf[copiedLen] = '\0';
    printk(KERN_INFO GENERAL_INFO_FMT "写入 %2ld 字节 成功.\t\t+[%s]\n", GENERAL_INFO(file, buffer), count, tempBuf);
    mutex_unlock(&mutex_write);

    return copiedLen;
}

static int __init chr_init(void) {
    int ret;
    struct device* dev_ret;

    /* 申请设备号 */
    if ((ret = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME)) < 0) {
        goto alloc_chrdev_region_err;
    }

    /* 初始化cdev，与file_operations、设备号相关联 */
    chr_dev.owner = THIS_MODULE;
    cdev_init(&chr_dev, &chr_fops);
    if ((ret = cdev_add(&chr_dev, dev_no, 1)) < 0) {
        goto cdev_add_err;
    }

    /* 创建类 */
    if (IS_ERR(dev_class = class_create(THIS_MODULE, DEVICE_NAME))) {
        ret = PTR_ERR(dev_class);
        goto class_create_err;
    }

    /* 创建设备节点 */
    if (IS_ERR(dev_ret = device_create(dev_class, NULL, dev_no, NULL, DEVICE_NAME))) {
        ret = PTR_ERR(dev_ret);
        goto device_create_err;
    }

    /* 申请缓冲区 */
    if ((ret = kfifo_alloc(&buffer, DEVICE_BUFFER_SIZE, GFP_KERNEL)) != 0) {
        goto kfifo_alloc_err;
    }

    /* 初始化等待队列 */
    init_waitqueue_head(&wq_read);
    init_waitqueue_head(&wq_write);

    /* 初始化互斥锁 */
    mutex_init(&mutex_read);
    mutex_init(&mutex_write);

    printk(KERN_INFO "[%s]: 设备初始化 成功: Major %d, Minor %d, BufferSize %d.\n",
           DEVICE_NAME, MAJOR(dev_no), MINOR(dev_no), DEVICE_BUFFER_SIZE);
    return 0;

kfifo_alloc_err:
    device_destroy(dev_class, dev_no);  // 移除设备节点
device_create_err:
    class_destroy(dev_class);  // 移除类
class_create_err:
    cdev_del(&chr_dev);  // 注销字符设备
cdev_add_err:
    unregister_chrdev_region(dev_no, 1);  // 注销设备号
alloc_chrdev_region_err:
    printk(KERN_INFO "[%s]: 设备初始化 失败.\n", DEVICE_NAME);
    return ret;

    // chr_dev.owner = THIS_MODULE;
    // alloc_chrdev_region(&dev_no, 0, 1, "ChrDev_XBA");            // 申请设备号
    // cdev_init(&chr_dev, &chr_fops);                              // 初始化cdev，与file_operations相关联
    // cdev_add(&chr_dev, dev_no, 1);                               // 将cdev与设备号关联
    // dev_class = class_create(THIS_MODULE, "ChrDev_XBA");         // 创建类
    // device_create(dev_class, NULL, dev_no, NULL, "ChrDev_XBA");  // 创建设备节点
    // pWrite = pRead = kmalloc(DEVICE_BUFFER_SIZE, GFP_KERNEL);    // 申请缓冲区
    // printk("Char device [ChrDev_XBA] init succeed: Major %d, Minor %d.\n", MAJOR(dev_no), MINOR(dev_no));
    // return 0;
}

static void __exit chr_exit(void) {
    kfifo_free(&buffer);                  // 释放缓冲区
    device_destroy(dev_class, dev_no);    // 移除设备节点
    class_destroy(dev_class);             // 移除类
    cdev_del(&chr_dev);                   // 注销字符设备
    unregister_chrdev_region(dev_no, 1);  // 注销设备号

    printk(KERN_INFO "[%s]: 设备卸载 成功.\n", DEVICE_NAME);
    return;
}

module_init(chr_init);
module_exit(chr_exit);
