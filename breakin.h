#ifndef _BREAKIN_H_
#define _BREAKIN_H_

#define MAX_DIMMS 32
#define MAX_CPUS 16
#define MAX_TEMPS 30
#define MAX_FANS 30
#define MAX_DISKS 24
#define MAX_BURNIN_SCRIPTS 64
#define MAX_NUM_NICS 32

#define DEFAULT_UPDATE_INTERVAL 300

time_t start_time;

int cpu_qty = 0;
int temp_qty = 0;
int fan_qty = 0;
int disk_qty = 0;
int burnin_script_count = 0;
int dimm_qty = 0;
int nic_qty = 0;

char update_url[1024] = "";
int update_interval = DEFAULT_UPDATE_INTERVAL;
int enable_updates = 0;

int serial_enabled = 0;
int serial_baud = 115200;
char serial_dev[1024] = "";

/* what tests are we disabling */
char breakin_disable[1024] = "";

struct nic_data_t {
	char		name[16];
	char		macaddr[32];
	char		macaddr_num[32];
	char		ipaddr[32];
} nic_data[MAX_NUM_NICS];

struct temp_data_t {
	char		name[16];
	double		value;
} temp_data[MAX_TEMPS];

struct fan_data_t {
	char		name[16];
	int		value;
} fan_data[MAX_FANS];


struct disk_info_t {
	char		device[30];
	double		size;
	double		performance;
} disk_info[MAX_DISKS];

struct cpu_usage_t {
	unsigned int		cpu_idle;
	unsigned int		cpu_usage;
	unsigned int		last_idle;
	unsigned int		last_usage;
	double 			percent;
} cpu_usage[MAX_CPUS];

struct cpu_info_t {
	int		qty;
	char		model[BUFSIZ];
	double		speed;
} cpu_info;

struct mem_info_t {
	long int 	mem_total;
	long int 	mem_free;
	double		mem_percent;
	long int 	swap_total;
	long int 	swap_free;
	double		swap_percent;
} mem_info;

struct burnin_scripts_t {
	char	name[BUFSIZ];
	char	path[BUFSIZ];
	char	last_msg[BUFSIZ];
	int	last_result;
	int	running;
	int	pid;
	FILE	*fp_stdout;
	FILE	*fp_stderr;
	long unsigned int pass_count;
	long unsigned int fail_count;
} burnin_scripts[MAX_BURNIN_SCRIPTS];

struct mem_ecc_t {
	int		memctrl;
	char		dimm_name[40];
	int		errors;
} mem_ecc[MAX_DIMMS];
#endif
