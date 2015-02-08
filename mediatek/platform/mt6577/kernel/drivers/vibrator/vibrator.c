
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include "timed_output.h"
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <mach/mt_typedefs.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_pwm.h>
#include <cust_gpio_usage.h>

#include <cust_vibrator.h>

#define VERSION					        "v 0.1"
#define VIB_DEVICE	        			"mt6575_vibrator"


#define RSUCCESS        0


/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INT			0x00000001	/* Interrupt related event */
#define DBG_EVT_TASKLET		0x00000002	/* Tasklet related event */

#define DBG_EVT_ALL			0xffffffff
 
#define DBG_EVT_MASK      	(DBG_EVT_TASKLET)

#if 1
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		printk(fmt, ##args); \
	} \
} while(0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __FUNCTION__)
#else
#define MSG(evt, fmt, args...) do{}while(0)
#define MSG_FUNC_ENTRY(f)	   do{}while(0)
#endif


static struct workqueue_struct *vibrator_queue;
static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static spinlock_t vibe_lock;
static int vibe_state;
static int ldo_state;

static int vibrator_pwm_beetlelite_num=PWM2;

extern void dct_pmic_VIBR_enable(kal_bool dctEnable);
extern int ispaon;// llf need extern;


extern void mt_power_on (U32 pwm_no);
extern void mt_power_off (U32 pwm_no);
extern S32 mt_set_pwm_enable ( U32 pwm_no ) ;
extern S32 mt_set_pwm_disable ( U32 pwm_no ) ;
extern int  get_call_status_value(void);

/***************chengui 20130419**********/
static int pwm_duration=115*20;
/***************chengui 20130419**********/

void pwm_vib_enable(int enable)
{
	
	printk("*wwl ****open pwm vibrator *********\n\r");

	if(enable)
	   	{
	 
				printk("open pwm vibrator \n\r");

				static struct pwm_spec_config pwm_setting;
				
				mt_set_gpio_mode(66, GPIO_MODE_01);

				//printk("set gpio68 is GPIO_MODE_01\n\r");

				pwm_setting.pwm_no = vibrator_pwm_beetlelite_num;
				pwm_setting.mode = PWM_MODE_FIFO; //new mode fifo and periodical mode

				pwm_setting.clk_div = CLK_DIV64;
				pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK;
                                /***************chengui 20130419**********/
				pwm_setting.PWM_MODE_FIFO_REGS.HDURATION = pwm_duration;
				pwm_setting.PWM_MODE_FIFO_REGS.LDURATION = pwm_duration;
                                /***************chengui 20130419**********/
				pwm_setting.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 31;
				pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x55555555;
				pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
				pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
				pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
				//pwm_setting.PWM_MODE_FIFO_REGS.GDURATION =47*32 ;

				printk("pwm_set_spec_config() start run pwm vibrator \n\r");
				pwm_set_spec_config(&pwm_setting);

	   }
	   else
	   	{
				printk("Shutdown pwm vibrator \n\r");
				mt_set_pwm_disable(vibrator_pwm_beetlelite_num);
				mt_pwm_power_off(vibrator_pwm_beetlelite_num);
	   	}

}


static int vibr_Enable(void)
{
	if(!ldo_state) {
		printk("[vibrator]vibr_Enable \n");
	/*	
		if(hwPowerOn(MT65XX_POWER_LDO_VIBR, VOL_2800, "VIBR")) {
			ldo_state=1;
		}
		*/
	//	dct_pmic_VIBR_enable(1);
	    //**********change vibrator from ldo to pwm by wwl*************//

	
	//	mdelay(100);
		
	    //power on camera IO
		pwm_vib_enable(1)	;

		if(ispaon==0)
			{
					//power on  MA1701
					mt_set_gpio_mode(GPIO_SPEAKER_EN_PIN,GPIO_MODE_00);  // gpio mode
					mt_set_gpio_pull_enable(GPIO_SPEAKER_EN_PIN,GPIO_PULL_ENABLE);
					mt_set_gpio_dir(GPIO_SPEAKER_EN_PIN,GPIO_DIR_OUT); // output
					mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high;
			}

			  
		ldo_state=1;
	}
	return 0;
}

static int vibr_Disable(void)
{
	if(ldo_state) {
		printk("[vibrator]vibr_Disable \n");
		/*
		if(hwPowerDown(MT65XX_POWER_LDO_VIBR, "VIBR")) {
			ldo_state=0;
		}
		*/
	//	dct_pmic_VIBR_enable(0);
	
	//**********change vibrator from ldo to pwm by wwl*************//

		
		if( ispaon==0)
			{
					//power off  MA1701
					mt_set_gpio_mode(GPIO_SPEAKER_EN_PIN,GPIO_MODE_00);  // gpio mode
					mt_set_gpio_pull_enable(GPIO_SPEAKER_EN_PIN,GPIO_PULL_ENABLE);
					mt_set_gpio_dir(GPIO_SPEAKER_EN_PIN,GPIO_DIR_OUT); // output
					mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
			}
		
	    //power off camera  IO
		pwm_vib_enable(0);
		ldo_state=0;
	}
   	return 0;
}


static void update_vibrator(struct work_struct *work)
{
	if(!vibe_state)
		vibr_Disable();
	else
		vibr_Enable();
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) 
	{
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		return ktime_to_ms(r);
	} 
	else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
		unsigned long   flags;

		spin_lock_irqsave(&vibe_lock, flags);
		while(hrtimer_cancel(&vibe_timer))
                {
                      printk("[vibrator]vibrator_enable: try to cancel hrtimer \n");
                }

		if (value == 0)
			vibe_state = 0;
		else 
		{
		if(value==10||value==30)  //virtual key feed back
			{
				value=value +50;
			}
		else if(value==20)  //google input methord

			{
				value=value+100;
			}
		else if(value==1||value==21) //boot up 
			{
				value=value+150;
			}
		else if(value==2||value==22) //longpress 
			{
				value=value+150;
			}
		else if(value==300)		//volume key down to silence
			{
				value=value+200;
			}
		else 								//others
			{
				value=value+180;
			}
		
			value = (value > 15000 ? 15000 : value);
			vibe_state = 1;
			hrtimer_start(&vibe_timer, 
							ktime_set(value / 1000, (value % 1000) * 1000000),
							HRTIMER_MODE_REL);
		}
		spin_unlock_irqrestore(&vibe_lock, flags);
                printk("[vibrator]vibrator_enable: vibrator start: %d \n", value); 
		queue_work(vibrator_queue, &vibrator_work);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
 		vibe_state = 0;
                printk("[vibrator]vibrator_timer_func: vibrator will disable \n");
 		queue_work(vibrator_queue, &vibrator_work);
 		return HRTIMER_NORESTART;
}

static struct timed_output_dev mt6575_vibrator = 
{
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int vib_probe(struct platform_device *pdev)
{
	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	if(vibe_state) {
		vibe_state = 0;
		vibr_Disable();
	}
}
static struct platform_driver vibrator_driver = 
{
    .probe		= vib_probe,
	.remove	    = vib_remove,
    .shutdown = vib_shutdown,
    .driver     = {
    .name = VIB_DEVICE,
    },
};

static ssize_t store_vibr_on(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	if(buf != NULL && size != 0)
	{
		printk("[vibrator]buf is %s and size is %d \n",buf,size);
		if(buf[0]== '0')
		{
			vibr_Disable();
		}else
		{
			vibr_Enable();
		}
	}
	return size;
}

static DEVICE_ATTR(vibr_on, 0664, NULL, store_vibr_on);


static s32 __devinit vib_mod_init(void)
{	
	s32 ret;

	printk("MediaTek MT6575 vibrator driver register, version %s\n", VERSION);

	vibrator_queue = create_singlethread_workqueue(VIB_DEVICE);
	if(!vibrator_queue) {
		printk("[vibrator]Unable to create workqueue\n");
		return -ENODATA;
	}
	INIT_WORK(&vibrator_work, update_vibrator);

	spin_lock_init(&vibe_lock);
	vibe_state = 0;
	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&mt6575_vibrator);

    ret = platform_driver_register(&vibrator_driver);

    if(ret) 
    {
		printk("[vibrator]Unable to register vibrator driver (%d)\n", ret);
		return ret;
    }	

	ret = device_create_file(mt6575_vibrator.dev,&dev_attr_vibr_on);
    if(ret)
    {
        printk("[vibrator]device_create_file vibr_on fail! \n");
    }
    
	printk("[vibrator]vib_mod_init Done \n");
 
    return RSUCCESS;
}

 
static void __exit vib_mod_exit(void)
{
	printk("MediaTek MT6575 vibrator driver unregister, version %s \n", VERSION);
	if(vibrator_queue) {
		destroy_workqueue(vibrator_queue);
	}
	printk("[vibrator]vib_mod_exit Done \n");
}

module_init(vib_mod_init);
module_exit(vib_mod_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MT6575 Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
