#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/time.h>

#define procfs_name "tsulab"
static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer,
                            size_t buffer_length, loff_t *offset) {
    struct timespec now;
    getnstimeofday(&now);
    now.tv_sec -= 499;  // ← Просто вычитаем 499 секунд (8 мин 19 сек)

    struct tm tm;
    time_to_tm(now.tv_sec, 0, &tm);

    char time_str[20];
    int len = snprintf(time_str, sizeof(time_str), "%02ld:%02ld:%02ld\n",
                      (long)tm.tm_hour, (long)tm.tm_min, (long)tm.tm_sec);

    if (*offset > 0)
        return 0;

    if (copy_to_user(buffer, time_str, len))
        return -EFAULT;

    *offset = len;
    return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

static int __init tsulab_init(void) {
    pr_info("Welcome to the Tomsk State University\n");
    our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
    if (!our_proc_file) {
        pr_err("Error: Could not initialize /proc/%s\n", procfs_name);
        return -ENOMEM;
    }
    pr_info("/proc/%s created\n", procfs_name);
    return 0;
}

static void __exit tsulab_exit(void) {
    proc_remove(our_proc_file);
    pr_info("Tomsk State University forever!\n");
}

module_init(tsulab_init);
module_exit(tsulab_exit);
MODULE_LICENSE("GPL");
