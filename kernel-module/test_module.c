#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sprintf.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/mutex.h>

static struct kobject *test_module;

static const char *LOG_DIR_PATH = "/var/tmp/test_module";

static uint interval_secs = 0;
static char msgs_filename[NAME_MAX + 1] = "messages";
static uint msgs_count;

static DEFINE_MUTEX(msgs_file_mutex);
static struct file *msgs_file;

static struct delayed_work write_msg_work;
static void write_msg_work_handler(struct work_struct *data)
{
	static char buf[64];
	int n = snprintf(buf, sizeof buf, "Hello from kernel module (%u)\n",
			 msgs_count);

	mutex_lock(&msgs_file_mutex);
	if (msgs_file == NULL) {
		return;
		mutex_unlock(&msgs_file_mutex);
	}
	while (n > 0) {
		int write_ret = kernel_write(msgs_file, buf, n, 0);
		if (write_ret < 0) {
			pr_err("write() failed: %d", write_ret);
			break;
		}
		n -= write_ret;
	}
	mutex_unlock(&msgs_file_mutex);

	++msgs_count;
	queue_delayed_work(system_wq, &write_msg_work, interval_secs * HZ);
}

static int mk_test_module_dir(void)
{
	struct path path;
	struct dentry *dentry =
		kern_path_create(AT_FDCWD, LOG_DIR_PATH, &path, LOOKUP_FOLLOW);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	int err = vfs_mkdir(&nop_mnt_idmap, d_inode(path.dentry), dentry,
			    S_IRUSR);
	done_path_create(&path, dentry);
	return err;
}

static struct file *open_msgs_file(const char *filename)
{
	int err = mk_test_module_dir();
	if (err && err != -EEXIST)
		return ERR_PTR(err);

	struct path log_dir_path;
	err = kern_path(LOG_DIR_PATH, LOOKUP_FOLLOW, &log_dir_path);
	if (err)
		return ERR_PTR(err);

	struct file *file = file_open_root(&log_dir_path, filename,
					   O_WRONLY | O_CREAT | O_APPEND,
					   S_IRUSR);
	path_put(&log_dir_path);
	return file;
}

static ssize_t interval_secs_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	int n = sysfs_emit(buf, "%u\n", interval_secs);
	return n;
}

static ssize_t interval_secs_store(struct kobject *kobj,
				   struct kobj_attribute *attr, const char *buf,
				   size_t count)
{
	uint value;
	int err = kstrtouint(buf, 10, &value);
	if (err < 0)
		return err;

	interval_secs = value;

	mutex_lock(&msgs_file_mutex);
	if (msgs_file == NULL) {
		struct file *newfile = open_msgs_file(msgs_filename);
		if (IS_ERR(newfile)) {
			mutex_unlock(&msgs_file_mutex);
			pr_err("failed to open file: %ld", PTR_ERR(newfile));
			// returning error code would only cause a confusion
			return 0;
		}
		msgs_file = newfile;
	}
	mutex_unlock(&msgs_file_mutex);

	if (value == 0)
		cancel_delayed_work_sync(&write_msg_work);
	else
		// mod_delayed_work(system_wq, &write_msg_work, value * HZ);
		mod_delayed_work(system_wq, &write_msg_work, value);

	return count;
}

static ssize_t filename_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return sysfs_emit(buf, "%s\n", msgs_filename);
}

static ssize_t filename_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	// validate, remove trailing newline and save filename

	int filename_count = count;
	if (count != 0 && buf[count - 1] == '\n')
		--filename_count;

	if (filename_count + 1 > sizeof msgs_filename)
		return -ENAMETOOLONG;

	for (int i = 0; i < filename_count; ++i)
		if (buf[i] == '/')
			return -EINVAL;

	memcpy(msgs_filename, buf, filename_count);
	msgs_filename[filename_count] = 0;

	// reopen file

	mutex_lock(&msgs_file_mutex);
	if (msgs_file != NULL) {
		filp_close(msgs_file, NULL);
		msgs_file = NULL;
	}
	struct file *newfile = open_msgs_file(msgs_filename);
	if (IS_ERR(newfile)) {
		mutex_unlock(&msgs_file_mutex);
		return PTR_ERR(newfile);
	}
	msgs_file = newfile;
	mutex_unlock(&msgs_file_mutex);

	return count;
}

static struct kobj_attribute interval_secs_attribute =
	__ATTR(interval_secs, 0644, interval_secs_show, interval_secs_store);
static struct kobj_attribute filename_attribute =
	__ATTR(filename, 0644, filename_show, filename_store);

static int __init test_module_init(void)
{
	int err = 0;

	test_module = kobject_create_and_add("test_module", kernel_kobj);
	if (!test_module) {
		err = -ENOMEM;
		goto out_err;
	}

	err = sysfs_create_file(test_module, &interval_secs_attribute.attr);
	if (err < 0)
		goto out_err;
	err = sysfs_create_file(test_module, &filename_attribute.attr);
	if (err < 0)
		goto out_err;

	INIT_DELAYED_WORK(&write_msg_work, write_msg_work_handler);

	return 0;

out_err:
	if (test_module != NULL)
		kobject_put(test_module);
	return err;
}

static void __exit test_module_exit(void)
{
	kobject_put(test_module);
	cancel_delayed_work_sync(&write_msg_work);
	if (msgs_file != NULL)
		filp_close(msgs_file, NULL);
}

module_init(test_module_init);
module_exit(test_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Appends a message to a text file periodically");
