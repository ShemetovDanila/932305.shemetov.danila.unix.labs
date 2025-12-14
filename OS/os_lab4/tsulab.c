#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define procfs_name "tsulab"
static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer,
                            size_t buffer_length, loff_t *offset) {
    char s[7] = "TSU\n";
    size_t len = strlen(s);
    
    if (*offset >= len)
        return 0;
    
    if (buffer_length < len - *offset)
        len = buffer_length;
    
    if (copy_to_user(buffer, s + *offset, len))
        return -EFAULT;
    
    *offset += len;
    pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);
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
MODULE_AUTHOR("TSU Student");
MODULE_DESCRIPTION("TSU OS Lab Module with /proc interface");
MODULE_VERSION("1.0");
