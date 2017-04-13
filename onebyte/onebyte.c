#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>

#define MAJOR_NUMBER 61

#define ONEBYTE_HELLO _IO(MAJOR_NUMBER, 0)
#define ONEBYTE_IOC_MAXNR 0

#define MAX_LEN 4194304  // length of onebyte_data: 4MB = 4 * 2^20 bytes

/* forward declaration */
int onebyte_open(struct inode *inode, struct file *filep);
int onebyte_release(struct inode *inode, struct file *filep);
ssize_t onebyte_read(struct file *filep, char *buf, size_t count, loff_t *f_pos);
ssize_t onebyte_write(struct file *filep, const char *buf, size_t count, loff_t *f_pos);
loff_t onebyte_llseek(struct file *filep, loff_t offset, int whence);
long onebyte_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static int onebyte_init(void);
static void onebyte_exit(void);

/* definition of file_operation structure */
struct file_operations onebyte_fops = {
    read:           onebyte_read,
    write:          onebyte_write,
    llseek:         onebyte_llseek,
    unlocked_ioctl: onebyte_ioctl,
    open:           onebyte_open,
    release:        onebyte_release
};

char *onebyte_data = NULL;
size_t len = 0;

int onebyte_open(struct inode *inode, struct file *filep)
{
    return 0;   // always successful
}

int onebyte_release(struct inode *inode, struct file *filep)
{
    return 0;   // always successful
}

ssize_t onebyte_read(struct file *filep, char *buf, size_t count, loff_t *f_pos)
{
    size_t read_len;
    
    // read() will be called again if return value is not 0
    // return 0 if reach end of file
    if (*f_pos >= len) {
        return 0;
    }
    
    read_len = len - *f_pos;
    
    if (count < read_len) {
        read_len = count;
    }
    
    if (copy_to_user(buf, &onebyte_data[*f_pos], read_len)) {
        return -EFAULT;
    }
    
    *f_pos += read_len;
    count -= read_len;
    
    return read_len;
}

ssize_t onebyte_write(struct file *filep, const char *buf, size_t count, loff_t *f_pos)
{
    size_t write_len, new_len;
    
    if (*f_pos >= MAX_LEN) {
        return -ENOSPC;
    } else if (*f_pos > len) {
        return -EFAULT;
    }
    
    write_len = MAX_LEN - *f_pos;
    
    if (count < write_len) {
        write_len = count;
    }
    
    if (copy_from_user(&onebyte_data[*f_pos], buf, write_len)) {
        return -EFAULT;
    }
    
    new_len = *f_pos + write_len;
    
    if (new_len > len) {
        len = new_len;
    }
    
    *f_pos += write_len;
    count -= write_len;
    
    return write_len;
}

loff_t onebyte_llseek (struct file *filep, loff_t offset, int whence)
{
    loff_t new_offset;
    
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = filep->f_pos + offset;
            break;
        case SEEK_END:
            new_offset = len - 1 + offset;
            break;
        default:
            return -EINVAL;
    }
    
    if (new_offset < 0 || new_offset >= len) {
        return -EINVAL;
    }
    
    filep->f_pos = new_offset;
    
    return new_offset;
}

long onebyte_ioctl (struct file *filep, unsigned int cmd, unsigned long arg)
{
    long ret_val;
    int err;
    
    ret_val = 0;
    err = 0;
    
    // extract the type and number bitfields, and don't decode
    // wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
    if (_IOC_TYPE(cmd) != MAJOR_NUMBER || _IOC_NR(cmd) > ONEBYTE_IOC_MAXNR) {
        return -ENOTTY;
    }
    
    // direction is a bitmask, and VERIFY_WRITE catches R/W
    // transfers. 'Type' is user-oriented, while
    // access_ok is kernel-oriented, so the concept of "read" and "write" is reversed
    if (_IOC_DIR(cmd) & _IOC_READ) {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    } else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }
    
    if (err) {
        return -EFAULT;
    }
    
    switch (cmd) {
        case ONEBYTE_HELLO:
            printk(KERN_WARNING "hello\n");
            break;
        default:
            // redundant, as cmd was checked against MAXNR
            return -ENOTTY;
    }
    
    return ret_val;
}

static int onebyte_init(void)
{
    int result;

    // register the device
    result = register_chrdev(MAJOR_NUMBER, "onebyte", &onebyte_fops);

    if (result < 0) {
        return result;
    }
    
    // Allocate one byte of memory for storage
    // kmalloc is just like malloc, the second parameter is
    // the type of memory to be allocated.
    // To release the memory allocated by kmalloc, use kfree.
    onebyte_data = kmalloc(MAX_LEN*sizeof(char), GFP_KERNEL);
    
    if (!onebyte_data) {
        onebyte_exit();
        
        // cannot allocate memory
        // return no memory error, negative signify a failure
        return -ENOMEM;
    }
    
    // initialize the value to be X
    *onebyte_data = 'X';
    len = 1;
    
    printk(KERN_ALERT "This is a onebyte device module\n");
    
    return 0;
}

static void onebyte_exit(void)
{
    // if the pointer is pointing to something
    if (onebyte_data) {
        // free the memory and assign the pointer to NULL
        kfree(onebyte_data);
        onebyte_data = NULL;
    }
    
    // unregister the device
    unregister_chrdev(MAJOR_NUMBER, "onebyte");
    
    printk(KERN_ALERT "Onebyte device module is unloaded\n");
}

MODULE_LICENSE("GPL");
module_init(onebyte_init);
module_exit(onebyte_exit);
