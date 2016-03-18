
/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */

#include <linux/timer.h>
#include <linux/string.h>
#include <linux/ctype.h>
//#include <linux/time.h>
//#include <stdarg/h>

MODULE_LICENSE("Dual BSD/GPL");

// Declaration of memory.c functions 
static int mytimer_open(struct inode *inode, struct file *filp);
static int mytimer_release(struct inode *inode, struct file *filp);
static ssize_t mytimer_read(struct file *filp,
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mytimer_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);
static void mytimer_exit(void);
static int mytimer_init(void);

// Structure that declares the usual file 
// access functions 
struct file_operations mytimer_fops = {
	read: mytimer_read,
	write: mytimer_write,
	open: mytimer_open,
	release: mytimer_release
};


//array of timers
struct Timers{
	struct timer_list my_timer;
	char mymessage[128];
};

struct Timers *ListTimers;
static char checkFlag[2] = {""}; //flag passed in
static int numTimers = 0;	//num of timers to print
static char  update[156];
static char *updated = update;//message for which timer update

// Declaration of the init and exit functions 
module_init(mytimer_init);
module_exit(mytimer_exit);

void my_timer_callback( unsigned long data )
{ 
  printk("%s\n", ListTimers[(int)data].mymessage);
  memset(ListTimers[(int)data].mymessage,0,sizeof(ListTimers[(int)data].mymessage));
}


static unsigned capacity = 128;
static unsigned bite = 128;
module_param(capacity, uint, S_IRUGO);
module_param(bite, uint, S_IRUGO);

// Global variables of the driver 
// Major number 
static int mytimer_major = 61;

// Buffer to store data 
static char *mytimer_buffer;
// length of the current message 
static int mytimer_len;



static int mytimer_init(void)
{
	int result;
	int i = 0;

	ListTimers = (struct Timers*)kmalloc(10*(sizeof(struct Timers)), GFP_KERNEL);
	if(!ListTimers)
		printk("Error with Timers struct");
	
	for(i = 0; i<10; i++)
	{ 	setup_timer( &ListTimers[i].my_timer, my_timer_callback, i);
		ListTimers[i].mymessage[0] = '\0';		
		}

	// Registering device 
	result = register_chrdev(mytimer_major, "mytimer", &mytimer_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"mytimer: cannot obtain major number %d\n", mytimer_major);
		return result;
	}

	// Allocating mytimer for the buffer 
	mytimer_buffer = kmalloc(capacity, GFP_KERNEL); 
	if (!mytimer_buffer)
	{ 
		printk(KERN_ALERT "Insufficient kernel memory\n"); 
		result = -ENOMEM;
		goto fail; 
	}
 
	memset(mytimer_buffer, 0, capacity);
	mytimer_len = 0;

	printk(KERN_ALERT "Inserting mytimer module\n");
 
	return 0;

fail: 
	mytimer_exit(); 
	return result;
}

static void mytimer_exit(void)
{	
	int ret;
	int i;
	for(i = 0; i<10; i++)
	{	ret = del_timer( &ListTimers[i].my_timer );
  		if(ret)
			printk("The timer is still in use...\n");
	}

  	printk("Timer module uninstalling\n");

	// Freeing the major number 
	unregister_chrdev(mytimer_major, "mytimer");

	// Freeing buffer memory 
	if (mytimer_buffer)
	{
		kfree(mytimer_buffer);
	}
	
	printk(KERN_ALERT "Removing mytimer module\n");

}

static int mytimer_open(struct inode *inode, struct file *filp)
{
	//printk(KERN_INFO "open called: process id %d, command %s\n",
	//	current->pid, current->comm);
	// Success 
	return 0;
}

static int mytimer_release(struct inode *inode, struct file *filp)
{
	//printk(KERN_INFO "release called: process id %d, command %s\n",
	//	current->pid, current->comm);
	// Success 
	return 0;
}

static ssize_t mytimer_read(struct file *filp, char *buf,
							size_t count, loff_t *f_pos)
{ 
	int temp;
	unsigned int timeLeft;
	char tbuf[256], *tbptr = tbuf;
	char wholebuf[256] = {""}, *wholeStr = wholebuf;
	int i;
	int j;
	int list[10][2];
	int holder[2];
	int num = 0;

	*f_pos -=1;
	// end of buffer reached 
	if (*f_pos >= mytimer_len)
	{	
		return 0;
	}

	// do not go over then end 
	if (count > mytimer_len - *f_pos)
		count = mytimer_len - *f_pos;

	// do not send back more than a bite 
	if (count > bite) count = bite;

	/* Transfering data to user space */ 

	tbptr += sprintf(tbptr,								   
		"read called: process id %d, command %s, count %d, chars ",
		current->pid, current->comm, count);

	for (temp = *f_pos; temp < count + *f_pos; temp++)					  
		tbptr += sprintf(tbptr, "%c", mytimer_buffer[temp]);

	//printk(KERN_INFO "%s\n", tbuf);

	// Changing reading position as best suits  
	*f_pos += count + 1; 

	//if flag -l
	if(!strcmp(checkFlag, "-l"))
	{
		//print all timers		
		if(numTimers == 0)
		{
			for(i = 0; i<10; i++)
			{
				if(strcmp(ListTimers[i].mymessage, ""))
				{	timeLeft = jiffies_to_msecs((ListTimers[i].my_timer.expires - jiffies))/1000;
					wholeStr += sprintf(wholeStr, "%s", ListTimers[i].mymessage);
					wholeStr += sprintf(wholeStr, " %u\n", timeLeft);
				}
			}
			count = strlen(wholebuf);
			if (copy_to_user(buf, wholebuf, strlen(wholebuf)))
			{
				return -EFAULT;
			}

			return count; 
		}
		//specific number of timers to print
		else
		{	//sort timers in order of closest to expiration
			for(i = 0; i<10; i++)
			{
				timeLeft = jiffies_to_msecs((ListTimers[i].my_timer.expires - jiffies))/1000;
				list[i][0] = timeLeft;
				list[i][1] = i;
			}
	
			for(i = 0; i < 9 ; i++)
			{
				for(j = 9; j>i; j--)
				{
					if((list[j][0])<(list[j-1][0]))
					{
						holder[0] = list[j][0];
						holder[1] = list[j][1];
						list[j][0] = list[j-1][0];
						list[j][1] = list[j-1][1];
						list[j-1][0] = holder[0];
						list[j-1][1] = holder[1];
					}
				}
			}

			for(i = 0; i<10; i++)
			{
				if(strcmp(ListTimers[i].mymessage, ""))
					num += 1;
			}
			if(num < numTimers)
				numTimers = num;
					
			for(i = 0; i<numTimers; i++)
			{
				wholeStr += sprintf(wholeStr, "%s", ListTimers[(list[i][1])].mymessage);
				wholeStr += sprintf(wholeStr, " %u\n", (list[i][0]));
			}
			count = strlen(wholebuf);
			if (copy_to_user(buf, wholebuf, strlen(wholebuf)))
			{
				return -EFAULT;
			}
			numTimers = 0;
			return count; 
		}
	}
	//if flag -s
	else
	{	count = strlen(update);
		if (copy_to_user(buf, updated, strlen(update)))
		{
			return -EFAULT;
		}
		memset(updated,0,sizeof(update));
		return count;
	}
}

static ssize_t mytimer_write(struct file *filp, const char *buf,
							size_t count, loff_t *f_pos)
{
	int ret;
	int temp;
	unsigned long time;
	char *endptr;
	char tbuf[256], *tbptr = tbuf;
	int i;
	char *updateStr;
	unsigned integer = 156;
	int timerIndex = 0;
	char *checkTimer;
	bool check = true;
	
	// end of buffer reached 
	if (*f_pos >= capacity)
	{
		printk(KERN_INFO
			"write called: process id %d, command %s, count %d, buffer full\n",
			current->pid, current->comm, count);
		return -ENOSPC;
	}

	// do not eat more than a bite 
	if (count > bite) count = bite;

	// do not go over the end 
	if (count > capacity - *f_pos)
		count = capacity - *f_pos;

	if (copy_from_user(mytimer_buffer + *f_pos, buf, count))
	{
		return -EFAULT;
	}

	tbptr += sprintf(tbptr,								   
		"write called: process id %d, command %s, count %d, chars ",
		current->pid, current->comm, count);

	for (temp = *f_pos; temp < count + *f_pos; temp++)					  
		tbptr += sprintf(tbptr, "%c", mytimer_buffer[temp]);

	//printk(KERN_INFO "%s\n", tbuf);

	*f_pos += count;
	mytimer_len = *f_pos;

	
	
	checkTimer = kmalloc(count, GFP_KERNEL);
	if(!checkTimer)
		printk("Error with checkTimer kmalloc");

	updateStr = kmalloc(integer, GFP_KERNEL);
	if(!updateStr)
		printk("Error with updateStr kmalloc");
	
	//get flag
	for(i = 0; i<2; i++)
	{
		checkFlag[i] = buf[i];

	}
	
	//if flag -s
	if(!strcmp(checkFlag, "-s"))
	{	//separte timer length and message
		sprintf(updateStr, "%s", buf+(3));
		for(i = 0; i<count-3; i++)
		{
			if(updateStr[i] == ' ')
			{	time = simple_strtoul(updateStr, &endptr, 10);
				strcpy(checkTimer, updateStr+(i+1));
				break;}	 
		}
		time = time*1000;

		//check if timer already exists
		for(i = 0; i<10; i++)
		{
			if(!strcmp(ListTimers[i].mymessage, checkTimer))
			{	timerIndex = i;
				check = false;
				break;}	 
		}
		
		//if timer doesn't exist
		if(check)
		{	//find index of timer and insert message
			for(i = 0; i<10; i++)
			{
				if(!strcmp(ListTimers[i].mymessage, ""))
				{	timerIndex = i;
					break;}	
			}	 
			strcpy(ListTimers[timerIndex].mymessage, checkTimer);
		}
		//set/modify timer
		ret = mod_timer( &ListTimers[timerIndex].my_timer, jiffies + msecs_to_jiffies(time) ); 
		
		//if timer exists, show it was updated
	  	if(ret) 
		{	memset(updated,0,sizeof(*updated));
			sprintf(updated, "The timer %s was updated!\n", ListTimers[timerIndex].mymessage);
		}
	}
	//if flag is -l
	else
	{	memset(updateStr,0,sizeof(*updateStr));
		sprintf(updateStr, "%s", buf+(3));
		numTimers = simple_strtoul(updateStr, &endptr, 10);
	}
	return count;
}
