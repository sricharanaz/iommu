 #include <linux/module.h>
 #include <linux/platform_device.h>
 #include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
 #include <linux/slab.h>
 #include <media/v4l2-ioctl.h>
 #include <media/v4l2-event.h>
#include <linux/io.h>

static struct kobject *example_kobject;
static struct kobject *crash_kobject;

int debug_globals[20] = {1, 2, 3 };
//extern void __iomem *debug_iommu_base;

static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr,
                      char *buf)
{
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,
                      char *buf, size_t count)
{
	int *p = 0;
	int i = 0, j = 0;
	volatile int loop = 1;

	if (!strcmp(buf, "1\n"))
		printk("debug module initialised successfully with value %s", buf);
	else if (!strcmp(buf, "2\n")) {
		*p = 10;
		printk("debug module crashed %s", buf);
	} else if (!strcmp(buf, "3\n")) {
		for (i = 0; i < 1000; i++) {
		   for (j = 0; j < 20; j++) {
			debug_globals[j] = i + j;
		   }
	       }
	} else if (!strcmp(buf, "4\n")) {
		while(loop);
	} else if (!strcmp(buf, "5\n")) {
		//writel_relaxed(0xFF, debug_iommu_base + 0x800);
	}
}

static struct kobj_attribute foo_attribute =__ATTR(foo, 0660, foo_show,
                                                   foo_store);

static int __init mymodule_init (void)
{
        int error = 0;

        printk("DEBUG Module initialized successfully \n");

        example_kobject = kobject_create_and_add("kobject_example",
                                                 kernel_kobj);
        if(!example_kobject)
                return -ENOMEM;

        error = sysfs_create_file(example_kobject, &foo_attribute.attr);
        if (error) {
                pr_debug("failed to create the foo file in /sys/kernel/kobject_example \n");
        }

        crash_kobject = kobject_create_and_add("kobject_crash",
                                                 kernel_kobj);
        if(!crash_kobject)
                return -ENOMEM;

        error = sysfs_create_file(crash_kobject, &foo_attribute.attr);
        if (error) {
                pr_debug("failed to create the foo file in /sys/kernel/crash_kobject \n");
        }

        return error;
}
module_init(mymodule_init);

static void __exit mymodule_exit (void)
{
        pr_debug ("Debug Module un initialized successfully \n");
        kobject_put(example_kobject);
}
module_exit(mymodule_exit);

MODULE_ALIAS("platform:qcom-debug-example-driver");
MODULE_DESCRIPTION("Qualcomm debug exmaple driver");

