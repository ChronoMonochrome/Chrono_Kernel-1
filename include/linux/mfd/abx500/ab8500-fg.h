#ifndef MFD_ABX500_AB8500_FG_H
#define MFD_ABX500_AB8500_FG_H

#include <linux/power_supply.h>

#define INS_CURR_TIMEOUT		(3 * HZ)

#if defined(CONFIG_MACH_JANICE)
#define USE_COMPENSATING_VOLTAGE_SAMPLE_FOR_CHARGING
#define FGRES_HWREV_02			133
#define FGRES_HWREV_02_CH		133
#define FGRES_HWREV_03			121
#define FGRES_HWREV_03_CH		120
#elif defined(CONFIG_MACH_CODINA)
#define USE_COMPENSATING_VOLTAGE_SAMPLE_FOR_CHARGING
#define FGRES				130
#define FGRES_CH			125
#elif defined(CONFIG_MACH_GAVINI)
#define USE_COMPENSATING_VOLTAGE_SAMPLE_FOR_CHARGING
#define FGRES				130
#define FGRES_CH			112
#else
#define FGRES				130
#define FGRES_CH			120
#endif

#define MAGIC_CODE			0x29
#define MAGIC_CODE_RESET		0x2F
#define OFF_MAGIC_CODE			25
#define OFF_MAGIC_CODE_MASK		0x3F

#define OFF_CHARGE_STATE		0
#define OFF_CAPACITY			5
#define OFF_VOLTAGE			12

#define OFF_CHARGE_STATE_MASK		0x1F
#define OFF_CAPACITY_MASK		0x7F
#define OFF_VOLTAGE_MASK		0x1FFF

#define LOWBAT_TOLERANCE		40
#define LOWBAT_ZERO_VOLTAGE		3320

#define MAIN_CH_NO_OVERSHOOT_ENA_N	0x02
#define MAIN_CH_ENA			0x01


#define MILLI_TO_MICRO			1000
#define FG_LSB_IN_MA			1627
#define QLSB_NANO_AMP_HOURS_X10		1129

#define SEC_TO_SAMPLE(S)		(S * 4)

#define NBR_AVG_SAMPLES			20

#define LOW_BAT_CHECK_INTERVAL		(5 * HZ)

#define VALID_CAPACITY_SEC		(45 * 60) /* 45 minutes */

#define VBAT_ADC_CAL			3700

#define CONFIG_BATT_CAPACITY_PARAM
#define BATT_CAPACITY_PATH		"/efs/last_battery_capacity"

#define WAIT_FOR_INST_CURRENT_MAX	70
#define IGNORE_VBAT_HIGHCUR	-500 /* -500mA */
#define CURR_VAR_HIGH	100 /*100mA*/
#define NBR_CURR_SAMPLES	10

#ifndef ABS
#define ABS(a) ((a) > 0 ? (a) : -(a))
#endif

#define interpolate(x, x1, y1, x2, y2) \
	((y1) + ((((y2) - (y1)) * ((x) - (x1))) / ((x2) - (x1))));

#define to_ab8500_fg_device_info(x) container_of((x), \
	struct ab8500_fg, fg_psy);

/**
 * struct ab8500_fg_interrupts - ab8500 fg interupts
 * @name:	name of the interrupt
 * @isr		function pointer to the isr
 */
struct ab8500_fg_interrupts {
	char *name;
	irqreturn_t (*isr)(int irq, void *data);
};

enum ab8500_fg_discharge_state {
	AB8500_FG_DISCHARGE_INIT,
	AB8500_FG_DISCHARGE_INITMEASURING,
	AB8500_FG_DISCHARGE_INIT_RECOVERY,
	AB8500_FG_DISCHARGE_RECOVERY,
	AB8500_FG_DISCHARGE_READOUT,
	AB8500_FG_DISCHARGE_WAKEUP,
};

enum ab8500_fg_charge_state {
	AB8500_FG_CHARGE_INIT,
	AB8500_FG_CHARGE_READOUT,
};

enum ab8500_fg_calibration_state {
	AB8500_FG_CALIB_INIT,
	AB8500_FG_CALIB_WAIT,
	AB8500_FG_CALIB_END,
};

struct ab8500_fg_avg_cap {
	int avg;
	int samples[NBR_AVG_SAMPLES];
	__kernel_time_t time_stamps[NBR_AVG_SAMPLES];
	int pos;
	int nbr_samples;
	int sum;
};
struct ab8500_fg_vbased_cap {
	int avg;
	int samples[NBR_AVG_SAMPLES];
	__kernel_time_t time_stamps[NBR_AVG_SAMPLES];
	int pos;
	int nbr_samples;
	int sum;
};

struct ab8500_fg_instant_current {
	int avg;
	int pre_sample;
	int samples[NBR_AVG_SAMPLES];
	__kernel_time_t time_stamps[NBR_AVG_SAMPLES];
	int nbr_samples;
	int high_diff_nbr;
	int sum;
	int pos;
};

struct ab8500_fg_battery_capacity {
	int max_mah_design;
	int max_mah;
	int mah;
	int permille;
	int level;
	int prev_mah;
	int prev_percent;
	int prev_level;
};

struct ab8500_fg_flags {
	bool fg_enabled;
	bool conv_done;
	bool charging;
	bool fully_charged;
	bool fully_charged_1st;
	bool chg_timed_out;
	bool low_bat_delay;
	bool low_bat;
	bool batt_unknown;
	bool calibrate;
};

enum cal_channels {
	ADC_INPUT_VMAIN = 0,
	ADC_INPUT_BTEMP,
	ADC_INPUT_VBAT,
	NBR_CAL_INPUTS,
};

struct adc_cal_data {
	u64 gain;
	u64 offset;
};

struct ab8500_gpadc {
	u8 chip_id;
	struct device *dev;
	struct list_head node;
	struct completion ab8500_gpadc_complete;
	struct mutex ab8500_gpadc_lock;
	struct regulator *regu;
	int irq;
	struct adc_cal_data cal_data[NBR_CAL_INPUTS];
	/*int regulator_enabled;*/
	int reference_count ;
	spinlock_t reference_count_spinlock ;
};

/**
 * struct ab8500_fg - ab8500 FG device information
 * @dev:		Pointer to the structure device
 * @vbat:		Battery voltage in mV
 * @vbat_adc:		Battery voltage in ADC
 * @vbat_nom:		Nominal battery voltage in mV
 * @inst_curr:		Instantenous battery current in mA
 * @avg_curr:		Average battery current in mA
 * @fg_samples:		Number of samples used in the FG accumulation
 * @accu_charge:	Accumulated charge from the last conversion
 * @recovery_cnt:	Counter for recovery mode
 * @high_curr_cnt:	Counter for high current mode
 * @init_cnt:		Counter for init mode
 * @recovery_needed:	Indicate if recovery is needed
 * @high_curr_mode:	Indicate if we're in high current mode
 * @vbat_cal_offset:	voltage offset  for calibarating vbat
 * @init_capacity:	Indicate if initial capacity measuring should be done
 * @calib_state		State during offset calibration
 * @discharge_state:	Current discharge state
 * @charge_state:	Current charge state
 * @node:		Struct of type list_head
 * @flags:		Structure for information about events triggered
 * @bat_cap:		Structure for battery capacity specific parameters
 * @avg_cap:		Average capacity filter
 * @parent:		Pointer to the struct ab8500
 * @gpadc:		Pointer to the struct gpadc
 * @pdata:		Pointer to the ab8500_fg platform data
 * @bat:		Pointer to the ab8500_bm platform data
 * @fg_psy:		Structure that holds the FG specific battery properties
 * @fg_wq:		Work queue for running the FG algorithm
 * @fg_periodic_work:	Work to run the FG algorithm periodically
 * @fg_low_bat_work:	Work to check low bat condition
 * @fg_work:		Work to run the FG algorithm instantly
 * @fg_reinit_work	Work used to reset and reinitialise the FG algorithm
 * @fg_acc_cur_work:	Work to read the FG accumulator
 * @cc_lock:		Mutex for locking the CC
 * @lowbat_wake_lock	Wakelock for low battery
 * @cc_wake_lock	Wakelock for Coulomb Counter
 */

struct ab8500_fg {
	struct device *dev;
	int vbat;
	int irq;
	int vbat_adc;
	int vbat_adc_compensated;
	int vbat_cal_offset;
	int vbat_nom;
	int inst_curr;
	int skip_add_sample;
	int n_skip_add_sample;
	int avg_curr;
	int fg_samples;
	int accu_charge;
	int recovery_cnt;
	int high_curr_cnt;
	int init_cnt;
	int new_capacity;
	int param_capacity;
	int prev_capacity;
	int gpadc_vbat_gain;
	int gpadc_vbat_offset;
	int gpadc_vbat_ideal;
	int smd_on;
	int reenable_charing;
	int fg_res_dischg;
	int fg_res_chg;
	int input_curr_reg;
	bool turn_off_fg;
	bool initial_capacity_calib;
	bool recovery_needed;
	bool high_curr_mode;
	bool init_capacity;
	bool reinit_capacity;
	bool lpm_chg_mode;
	bool lowbat_poweroff;
	bool lowbat_poweroff_locked;
	bool max_cap_changed;
	enum ab8500_fg_calibration_state calib_state;
	enum ab8500_fg_discharge_state discharge_state;
	enum ab8500_fg_charge_state charge_state;
	struct list_head node;
	struct completion ab8500_fg_complete;
	struct ab8500_fg_flags flags;
	struct ab8500_fg_battery_capacity bat_cap;
	struct ab8500_fg_vbased_cap vbat_cap;
	struct ab8500_fg_instant_current inst_cur;
	struct ab8500_fg_avg_cap avg_cap;
	struct ab8500 *parent;
	struct ab8500_gpadc *gpadc;
	struct ab8500_fg_platform_data *pdata;
	struct ab8500_bm_data *bat;
	struct power_supply fg_psy;
	struct workqueue_struct *fg_wq;
	struct delayed_work fg_periodic_work;
	struct delayed_work fg_low_bat_work;
	struct delayed_work fg_reinit_work;
	struct delayed_work fg_reinit_param_work;
	struct work_struct fg_work;
	struct work_struct fg_acc_cur_work;
	struct notifier_block fg_notifier;
	struct mutex cc_lock;
	struct wake_lock lowbat_wake_lock;
	struct wake_lock lowbat_poweroff_wake_lock;
	struct wake_lock cc_wake_lock;
};

#endif /* MFD_ABX500_AB8500_FG_H */
