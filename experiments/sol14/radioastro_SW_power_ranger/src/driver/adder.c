/*** Author  :   William Simon <william.simon@epfl.ch>
 ***             Miguel Peon <miguel.peon@epfl.ch>
 *** Date    :   March 2021
 ***/

#include <linux/init.h>          /* needed for module_init and exit */
#include <linux/module.h>
#include <linux/moduleparam.h>   /* needed for module_param */
#include <linux/kernel.h>        /* needed for printk */
#include <linux/types.h>         /* needed for dev_t type */
#include <linux/kdev_t.h>        /* needed for macros MAJOR, MINOR, MKDEV... */
#include <linux/fs.h>            /* needed for register_chrdev_region, file_operations */
#include <linux/interrupt.h>
#include <linux/cdev.h>          /* cdev definition */
#include <linux/slab.h>		       /* kmalloc(),kfree() */
#include <asm/uaccess.h>         /* copy_to copy_from _user */
#include <linux/uaccess.h>
#include <linux/io.h>

#define DRIVER_NAME "adder_driver"
#define ADDER_IRQ 48  // Hard-coded value of IRQ vector (GIC: 61).

// Structure that mimics the layout of the peripheral registers.
// Vitis HLS skips some addresses in the register file. We introduce
// padding fields to create the right mapping to registers with our structure,
struct TRegs {
  uint32_t control; // 0x00
  uint32_t gier, ier, isr; // 0x04, 0x08, 0x0C
  uint32_t input ; // 0x10
  uint32_t input_h; // 0x14
  uint32_t padding0; // 0x18
  uint32_t output; // 0x1C
  uint32_t output_h; // 0x20
  uint32_t padding1; // 0x24

  uint32_t numFFT; // 0x28
  uint32_t padding2; // 0x2C
};

// Structure used to pass commands between user-space and kernel-space.
struct user_message {
  uint32_t input;
  uint32_t output;
 
  uint32_t numFFT;

};

int adder_major = 0;
int adder_minor = 0;
module_param(adder_major,int,S_IRUGO);
module_param(adder_minor,int,S_IRUGO);

// We declare a wait queue that will allow us to wait on a condition.
wait_queue_head_t wq;
int flag = 0;

// This structure contains the device information.
struct adder_info {
  int irq;
  unsigned long memStart;
  unsigned long memEnd;
  void __iomem  *baseAddr;
  struct cdev   cdev;            /* Char device structure               */
};

static struct adder_info adder_mem = {ADDER_IRQ, 0x40000000, 0x4000FFFF};

// Declare here the user-accessible functions that the driver implements.
int adder_open(struct inode *inode, struct file *filp);
int adder_release(struct inode *inode, struct file *filed_mem);
ssize_t adder_read(struct file *filed_mem, char __user *buf, size_t count, loff_t *f_pos);

// IRQ handler function.
static irq_handler_t  adderIRQHandler(unsigned int irq, void *dev_id, struct pt_regs *regs);

// This structure declares the operations that our driver exports for the users.
struct file_operations adder_fops = {
  .owner =    THIS_MODULE,
  .read =     adder_read,
  .open =     adder_open,
  .release =  adder_release,
};


// Function that implements system call open() for our driver.
// Initialize the device and enable the interrups here.
int adder_open(struct inode *inode, struct file *filp)
{
  pr_info("ADDER_DRIVER: Performing 'open' operation\n");
  return 0;         
}

// Function that implements system call release() for our driver.
// Used with close() or when the OS closes the descriptors held by
// the process when it is closed (e.g., Ctrl-C).
// Stop the interrupts and disable the device.
int adder_release(struct inode *inode, struct file *filed_mem)
{
  pr_info("ADDER_DRIVER: Performing 'release' operation\n");
  return 0;
}

// The cleanup function is used to handle initialization failures as well.
// Thefore, it must be careful to work correctly even if some of the items
// have not been initialized

void adder_cleanup_module(void)
{
  dev_t devno = MKDEV(adder_major, adder_minor);
  disable_irq(adder_mem.irq);
  free_irq(adder_mem.irq,&adder_mem);
  iounmap(adder_mem.baseAddr);
  release_mem_region(adder_mem.memStart, adder_mem.memEnd - adder_mem.memStart + 1);
  cdev_del(&adder_mem.cdev);
  unregister_chrdev_region(devno, 1);        /* unregistering device */
  pr_info("ADDER_DRIVER: Cdev deleted, adder device unmapped, chdev unregistered\n");
}

// Function that implements system call read() for our driver.
// Returns 1 uint32_t with the number of times the interrupt has been detected.
ssize_t adder_read(struct file *filed_mem, char __user *buf, size_t count, loff_t *f_pos)
{
  volatile struct TRegs * slave_regs = (struct TRegs*)adder_mem.baseAddr;
  struct user_message message;
  uint32_t status;

  if (count < sizeof(struct user_message)) {
    pr_err("ADDER_DRIVER: USer buffer too small (> %d bytes).\n", sizeof(struct user_message));
    return -1;
  }

  // Copy the information from user-space to the kernel-space buffer.
  if(raw_copy_from_user(&message, buf, sizeof(struct user_message)))
  {
    pr_err("ADDER_DRIVER: Raw copy from user buffer failed.\n");
    return -1;
  }

  // Program the peripheral registers.
  iowrite32(message.input, (volatile void*)(&slave_regs->input));
  iowrite32(message.output, (volatile void*)(&slave_regs->output));
  iowrite32(message.numFFT, (volatile void*)(&slave_regs->numFFT));

  
  // Enable interrupts (global and spacific to done).
  iowrite32(1, (volatile void*)(&slave_regs->gier));
  iowrite32(1, (volatile void*)(&slave_regs->ier));
  mb();
  pr_info("ADDER_DRIVER: Starting accel...\n");
  
  // Tell the peripheral to start (start bit = 1)
  status = ioread32((volatile void*)(&slave_regs->control));
  status |= 1; 
  iowrite32(status, (volatile void*)(&slave_regs->control));
  mb();

  // blocking read (PS user application goes to sleep)
  // Sleep the thread until the peripheral generates an interrupt
  // wait_event_interruptible may exit when a signal is received, so
  // we check our flag to ensure that it was our own interrupt handler
  // waking up us after the interrupt is received, and not an 
  // spurious signal.
  // When we go to sleep, the processor is free for other tasks.
  flag = 0;
  while(wait_event_interruptible(wq, flag !=0)) {
    printk(KERN_ALERT "ADDER_DRIVER: AWOKEN BY ANOTHER SIGNAL\n");
  }
  pr_info("ADDER_DRIVER: AWOKEN FROM INTERRUPT\n");

  // Disable interrupts.
  iowrite32(0, (volatile void*)&slave_regs->gier);
  iowrite32(0, (volatile void*)&slave_regs->ier);
  mb();

  pr_info("ADDER_DRIVER: Performed READ operation successfully\n");
  return 0;
}

// Set up the char_dev structure for this device.
static void adder_setup_cdev(struct adder_info *_adder_mem)
{
	int err, devno = MKDEV(adder_major, adder_minor);

	cdev_init(&_adder_mem->cdev, &adder_fops);
	_adder_mem->cdev.owner = THIS_MODULE;
	_adder_mem->cdev.ops = &adder_fops;
	err = cdev_add(&_adder_mem->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		pr_err("ADDER_DRIVER: Error %d adding adder cdev_add", err);

  pr_info("ADDER_DRIVER: Cdev initialized\n");
}


// The init function registers the chdev.
// It allocates dynamically a new major number.
// The major number corresponds to a different function driver.
static int adder_init(void)
{
  int result = 0;
  dev_t dev = 0;

  // Allocate a function number for our driver (major number).
  // The minor number is the instance of the driver.
  pr_info("ADDER_DRIVER: Allocating a new major number.\n");
  result = alloc_chrdev_region(&dev, adder_minor, 1, "adder");
  adder_major = MAJOR(dev);
  if (result < 0) {
    pr_err("ADDER_DRIVER: Can't get major %d\n", adder_major);
    return result;
  }

  // Request (exclusive) access to the memory address range of the peripheral.
  if (!request_mem_region(adder_mem.memStart, adder_mem.memEnd - adder_mem.memStart + 1, DRIVER_NAME)) {
    pr_err("ADDER_DRIVER: Couldn't lock memory region at %p\n", (void *)adder_mem.memStart);
    unregister_chrdev_region(dev, 1);
    return -1;
  }

  // Obtain a "kernel virtual address" for the physical address of the peripheral.
  adder_mem.baseAddr = ioremap(adder_mem.memStart, adder_mem.memEnd - adder_mem.memStart + 1);
  if (!adder_mem.baseAddr) {
    pr_err("ADDER_DRIVER: Could not obtain virtual kernel address for iomem space.\n");
    release_mem_region(adder_mem.memStart, adder_mem.memEnd - adder_mem.memStart + 1);
    unregister_chrdev_region(dev, 1);
    return -1;
  }

  init_waitqueue_head(&wq);

  // Request registering our interrupt handler for the IRQ of the peripheral.
  // We configure the interrupt to be detected on the rising edge of the signal.
  result = request_irq(adder_mem.irq, (irq_handler_t)adderIRQHandler, IRQF_TRIGGER_RISING, DRIVER_NAME, &adder_mem);
  if(result) {
    printk(KERN_ALERT "ADDER_DRIVER: Failed to register interrupt handler (error=%d)\n", result);     
    iounmap(adder_mem.baseAddr);
    release_mem_region(adder_mem.memStart, adder_mem.memEnd - adder_mem.memStart + 1);
    cdev_del(&adder_mem.cdev);
    unregister_chrdev_region(dev, 1);
    return result;
  }

  // Enable the IRQ. From this moment on, we can receive the IRQ asynchronously at any time.
  enable_irq(adder_mem.irq);
  pr_info("ADDER_DRIVER: Interrupt %d registered\n", adder_mem.irq);

  pr_info("ADDER_DRIVER: driver at 0x%08X mapped to 0x%08X\n",
    (uint32_t)adder_mem.memStart, (uint32_t)adder_mem.baseAddr); 
  adder_setup_cdev(&adder_mem);

  return 0;
}


// The exit function calls the cleanup
static void adder_exit(void)
{
	  pr_info("ADDER_DRIVER: calling cleanup function.\n");
	  adder_cleanup_module();
}

// Declare init and exit handlers.
// They are invoked when the driver is loaded or unloaded.
module_init(adder_init);
module_exit(adder_exit);


// The interrupt handler is called on the (rising edge of the) accelerator interrupt.
// The interrupt handler is executed in an interrupt context, not a process context!!!
// It must be quick, it cannot sleep. It cannot use functions that can sleep
// (e.g., don't allocate memory if that may wait for swapping).
// The handler cannot communicate directly with the user-space. The user-space does not
// interact with the interrupt handler.
static irq_handler_t adderIRQHandler(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
  volatile struct TRegs * slave_regs = (struct TRegs*)adder_mem.baseAddr;
  // Clean the interrupt in the peripheral, so that we can detect new rising transition.
  // The ISR is toggle-on-write (TOW), which means that its bits toggle when they are
  // written, whatever it was their previous value. Therefore, we write (1) to the 
  // 'done' bit to toggle it, so that it becomes 0 and the interrupt is disarmed.
  iowrite32(1, (volatile void*)&slave_regs->isr);
  mb();

  // Signal that it is us waking the main thread.
	flag = 1;
  // Wake the main thread.
	wake_up_interruptible(&wq);
	return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
  // In case of error, or if it was not our device which generated the IRQ, return IRQ_NONE.
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ruben Rodriguez Alvarez <ruben.rodriguezalvarez@epfl.ch>");
MODULE_DESCRIPTION("Example device driver for controlling PYNQ-Z2 SimpleVectorAdder");
MODULE_VERSION("1.0");



