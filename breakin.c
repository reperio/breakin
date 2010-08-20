#include <ncurses.h>
#include <panel.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <pthread.h>
#include <curl/curl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <libgen.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>


#include "util.h"
#include "breakin.h"
#include "dmidecode.h"
#include "benchmarks.h"
#include "bench_stream.h"
#include "bench_disk.h"

#define SLEEP_DELAY 250000000	/* how long do we sleep during update cycles */
#define THREAD_DELAY 200000000	/* how long do we sleep during update cycles */
#define KEY_DELAY 10000000	/* how long do we sleep during update cycles */

#define LOGDEV_DELAY 30		/* how long between output to the logdevice / serial port*/

#define STAT_MIN_ROWS 10
#define ERR_LOG_FILE "/var/log/breakin.error.log"
#define PRODUCT_NAME "Advanced Clustering Breakin"
#define SENSOR_PATH "/sys/class/hwmon"
#define STAT_FILE "/var/run/breakin.dat"
#define BLOCK_DEV_PATH "/sys/block"
#define BURNIN_SCRIPT_PATH "/etc/breakin/tests"

#define LOG_DEV "/var/logdev"
#define LOG_FILE "/var/log/breakin.log"
#define LOG_DEV_FILE "/var/logdev/breakin.log"

#define ESCAPE 27

/* thresholds to change temperature colors */
#define TEMP_YELLOW 50
#define TEMP_RED 60

extern int errno;

WINDOW *create_newwin(int height, int width, int starty, int startx);
WINDOW *create_subwin(WINDOW *parent, int height, int width, int starty, int startx);

long log_pos = -1;
long syslog_pos = -1;

WINDOW *main_win, *log_win, *stat_win, *details_win, *details_border, *bottom_bar;
PANEL *main_panel, *details_panel;

FILE *log_file, *logdev_file, *serial_file;
int serial_fd;

int hardware_finished = 0;
int show_details_win = 0;
int enable_benchmarks = 1;
int enabled_ssh = 0;

int log_message(char *message) {

	char cur_time[BUFSIZ];

	running_time(cur_time);

	wattron(log_win,COLOR_PAIR(2));
	wprintw(log_win, "%s: %s\n", cur_time, message);
	
	if (log_file != NULL) {
		fprintf(log_file, "MSG %s %s\n\n", cur_time, message);
		fflush(log_file);
	}
	if (logdev_file != NULL) {
		fprintf(logdev_file, "MSG %s %s\n", cur_time, message);
		fflush(logdev_file);
	}
	if (serial_enabled && serial_file != NULL) {
		fprintf(serial_file, "MSG %s %s\n", cur_time, message);
		fflush(serial_file);
	}

	wattroff(log_win,COLOR_PAIR(2));
	wrefresh(log_win);

	return 0;
}

	
int log_error(char *message) {

	char cur_time[BUFSIZ];

	running_time(cur_time);

	wattron(log_win,COLOR_PAIR(2));
	wprintw(log_win, "%s: ", cur_time);
	wattroff(log_win,COLOR_PAIR(2));

	wattron(log_win, COLOR_PAIR(1));
	wprintw(log_win, "%s\n", message);

	if (log_file != NULL) {
		fprintf(log_file, "ERR %s %s\n", cur_time, message);
		fflush(log_file);
	}

	if (logdev_file != NULL) {
		fprintf(logdev_file, "ERR %s %s\n", cur_time, message);
		fflush(logdev_file);
	}
	if (serial_enabled && serial_file != NULL) {
		fprintf(serial_file, "ERR %s %s\n", cur_time, message);
		fflush(serial_file);
	}

	wattroff(log_win, COLOR_PAIR(1));
	wrefresh(log_win);

	return 0;
}

int monitor_syslog() {

	FILE *fp;
	char buf[BUFSIZ], cur_time[BUFSIZ];

	fp = fopen("/var/log/messages", "r");
	if (fp == NULL) {
		return 1;
	}

	if (syslog_pos == -1) {
		fseek(fp, 0L, SEEK_END);
	}
	else {
		fseek(fp, syslog_pos, SEEK_CUR);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		trim(buf);
		if (strstr(buf, ".err") != NULL) {
			log_error(buf);
		}
		else if (strstr(buf, ".emerg") != NULL) {
			log_error(buf);
		}
	}
	syslog_pos = ftell(fp);
	fclose(fp);
}

int show_log(WINDOW *win) {

	FILE *fp;
	char buf[BUFSIZ], cur_time[BUFSIZ];

	fp = fopen(ERR_LOG_FILE, "r");
	if (fp == NULL) {
		return 1;
	}

	if (log_pos == -1) {
		fseek(fp, 0L, SEEK_END);
	}
	else {
		fseek(fp, log_pos, SEEK_CUR);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {

		trim(buf);
		log_error(buf);
		wrefresh(win);
	}
	log_pos = ftell(fp);
	fclose(fp);
}

size_t upload_handle_response(void *ptr, size_t size, size_t nmeb, 
	void *stream) {

	if (strncmp(ptr, "OK", 2) == 0) {
	}
	else {
	}

	return (size * nmeb);
}

int upload_data(char *id, int interval, char *url, char *datafile, char *logfile) {

	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	char error_msg[CURL_ERROR_SIZE] = "";
	long code;
	char cur_time[BUFSIZ], buf[BUFSIZ];
	int run_time;

	run_time = time(NULL) - start_time;

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "data",
		CURLFORM_FILE, datafile, CURLFORM_END);

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "log",
		CURLFORM_FILE, logfile, CURLFORM_END);

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "id",
		CURLFORM_COPYCONTENTS, id, CURLFORM_END);

	sprintf(buf, "%d", run_time);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "time",
		CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	sprintf(buf, "%d", interval);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "interval",
		CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, url);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_msg);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, upload_handle_response);

	res = curl_easy_perform(curl);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_formfree(formpost);
	curl_easy_cleanup(curl);

	return 0;
}


int do_update() {
	struct timespec ts;
	char id[BUFSIZ] = "";
	int i = 0, found = 0;

	while (1) {

		ts.tv_sec = update_interval;
		ts.tv_nsec = 0;


		for (i = 0; i < nic_qty; i ++) {
			if ((strncmp(nic_data[i].name, "eth", 3) == 0) ||
			    (strncmp(nic_data[i].name, "ath", 3) == 0)) {
				sprintf(id, "%s", nic_data[i].macaddr_num);
				found = 1;
				break;
			}
		}

		/* if we haven't found a valid ethernet interface we try again */
		if (!found) {
			nanosleep(&ts, NULL);
			continue;
		}

		/* log_message("Sending status to the server"); */
		upload_data(id, update_interval, update_url, STAT_FILE, LOG_FILE);
		nanosleep(&ts, NULL);
	}
}

double get_cpu_usage() {

	FILE *fp;
	char buf[BUFSIZ], cpu[12], label[20];
	unsigned int cpu_user, cpu_nice, cpu_sys, cpu_idle; 
	unsigned int time_idle, time_used, time_total;
	int cpu_count = 0;
	double cpu_percent;

	memset(buf, sizeof(buf), '\0');

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		return 1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strncmp(buf, "cpu", 3) == 0) {

			if (cpu_count < MAX_CPUS) {

				sscanf(buf, "%s %d %d %d %d", &cpu, &cpu_user, 
					&cpu_nice, &cpu_sys, &cpu_idle);

				cpu_usage[cpu_count].cpu_usage = 
					cpu_user + cpu_nice + cpu_sys;
				cpu_usage[cpu_count].cpu_idle = cpu_idle;
			
				time_idle = cpu_idle - 
					cpu_usage[cpu_count].last_idle;	

				time_used = cpu_usage[cpu_count].cpu_usage - 
					cpu_usage[cpu_count].last_usage;	

				time_total = time_idle + time_used;	
				cpu_percent = ((double) time_used / time_total) * 100;

				cpu_usage[cpu_count].last_idle = cpu_idle;
				cpu_usage[cpu_count].last_usage = 
					cpu_usage[cpu_count].cpu_usage;

				cpu_usage[cpu_count].percent = cpu_percent;
				cpu_count ++;
			}
		}
		memset(buf, sizeof(buf), '\0');
	}
	cpu_qty = cpu_count;
	fclose(fp);
}

int get_cpu_info() {
	FILE *fp;
	char buf[BUFSIZ];
	double cpu_mhz;
	int cpucount = 0, pos;

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL) {
		return 1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strncmp(buf, "processor", 9) == 0) {
			cpucount ++;
		}
		else if (strncmp(buf, "model name", 10) == 0) {
			sprintf(cpu_info.model, "%s", trim(strstr(buf, ":") + 2));
		}
		else if (strncmp(buf, "cpu MHz", 7) == 0) {
			cpu_info.speed = atof(strstr(buf, ":") + 2);
		}
	}
	fclose(fp);
	cpu_info.qty = cpucount;
}

int get_nic_info() {

	struct ifconf   Ifc;
	struct ifreq    IfcBuf[MAX_NUM_NICS];
	struct ifreq    *pIfr;

	int i = 0;
	int fd = 0;
	int nics = 0;
	int num_ifreq = 0;
 
	Ifc.ifc_len = sizeof(IfcBuf);
	Ifc.ifc_buf = (char *) IfcBuf;

	if (! hardware_finished) {
		return;
	}
 	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		return(1);
	}
	if (ioctl(fd, SIOCGIFCONF, &Ifc) < 0) {
		log_message(strerror(errno));
		close(fd);
		return(1);
	}
	num_ifreq = Ifc.ifc_len / sizeof(struct ifreq);
	for ( pIfr = Ifc.ifc_req, i = 0 ; i < num_ifreq; pIfr++, i++ ) {
		struct ifreq req, ifr;
		struct sockaddr_in *ifaddr;

		sprintf(req.ifr_name, "%s", pIfr->ifr_name);
		if (ioctl(fd, SIOCGIFHWADDR, &req) == -1) {
			continue;
		}

		if (strncmp("lo", pIfr->ifr_name, 2) == 0) {
			continue;
		}

		snprintf(nic_data[nics].name, sizeof(nic_data[nics].name), 
			"%s", pIfr->ifr_name);

 		sprintf(nic_data[nics].macaddr, "%02x:%02x:%02x:%02x:%02x:%02x",
			(int)(req.ifr_hwaddr.sa_data[0] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[1] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[2] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[3] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[4] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[5] & 0xff));

 		sprintf(nic_data[nics].macaddr_num, "%02x%02x%02x%02x%02x%02x",
			(int)(req.ifr_hwaddr.sa_data[0] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[1] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[2] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[3] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[4] & 0xff),
			(int)(req.ifr_hwaddr.sa_data[5] & 0xff));

		memset(&ifr, 0, sizeof(struct ifreq));
		strncpy(ifr.ifr_name, pIfr->ifr_name, IF_NAMESIZE);
		if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
			continue;
		}
		else {
	  		ifaddr = (struct sockaddr_in *)&ifr.ifr_addr;
			sprintf(nic_data[nics].ipaddr, "%s", inet_ntoa(ifaddr->sin_addr));
		}

		nics ++;
	}
	close(fd);

	nic_qty = nics;
}

double get_mem_usage() {
	FILE *fp;
	char buf[BUFSIZ];
	long int mem_total, mem_free, swap_total, swap_free;
	double mem_percent, swap_percent;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		return 1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {

		if (strncmp(buf, "MemTotal", 8) == 0) {
			sscanf(buf, "%*s %ld %*s", &mem_info.mem_total);
		}
		else if (strncmp(buf, "MemFree", 7) == 0) {
			sscanf(buf, "%*s %ld %*s", &mem_info.mem_free);
		}
		else if (strncmp(buf, "SwapTotal", 9) == 0) {
			sscanf(buf, "%*s %ld %*s", &mem_info.swap_total);
		}
		else if (strncmp(buf, "SwapFree", 8) == 0) {
			sscanf(buf, "%*s %ld %*s", &mem_info.swap_free);
		}
	}
	fclose(fp);

	mem_info.mem_percent = ((double) 
		(mem_info.mem_total - mem_info.mem_free) / mem_info.mem_total) * 100;

	mem_info.swap_percent = ((double) 
		(mem_info.swap_total - mem_info.swap_free) / mem_info.swap_total) * 100;
}

int get_ipmi_sensors() {
	FILE *fp;
	char buf[BUFSIZ];
	char label[20]; 
	int value = 0;
	char *chunk, *string;
	fan_qty = 0;
	temp_qty = 0;

	fp = fopen("/tmp/ipmi_temp.log", "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			int i  = 0, found_label = 0, found_value = 0;
			string = buf;	
			while (string) {
				chunk = strsep(&string, "|");
				/* label */
				if (i == 0) {
					snprintf(label, sizeof(label), "%s", trim(chunk));
					found_label = 1;
				}
				/* value */
				else if (i == 1) {
					sscanf(chunk, "%d %*s", &value);	
					found_value = 1;
				}
				i ++;
			}	
			if (found_label && found_value) {
				if (temp_qty < MAX_TEMPS) {
					snprintf(temp_data[temp_qty].name, sizeof(temp_data[temp_qty].name), "%s", label);
					temp_data[temp_qty].value = value;
					temp_qty ++;
				}
			}
		}
		fclose(fp);
	}

	fp = fopen("/tmp/ipmi_fan.log", "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			int i  = 0, found_label = 0, found_value = 0;
			string = buf;	
			while (string) {
				chunk = strsep(&string, "|");
				/* label */
				if (i == 0) {
					snprintf(label, sizeof(label), "%s", trim(chunk));
					found_label = 1;
				}
				/* value */
				else if (i == 1) {
					sscanf(chunk, "%d %*s", &value);	
					found_value = 1;
				}
				i ++;
			}	
			if (found_label && found_value) {
				if (temp_qty < MAX_FANS) {
					snprintf(fan_data[fan_qty].name, sizeof(fan_data[fan_qty].name), "%s", label);
					fan_data[fan_qty].value = value;
					fan_qty ++;
				}
			}
		}
		fclose(fp);
	}
	return 0;
}

int read_string_from_file(char *filename, char *data) {
	FILE *fp;

	fp = fopen(filename,"r");
	if (fp == NULL) {
		return 0;
	}

	fscanf(fp, "%s", data);
	data = trim(data);

	fclose(fp);

	return 1;
}

int get_hd_info() {
	DIR *block_dir;
	FILE *fp;
	char filename[BUFSIZ], buf[BUFSIZ];
	int diskcount = 0, error;
	double disk_size;
	struct dirent *dir_entry, *de;
	

	block_dir = opendir(BLOCK_DEV_PATH);
	if(block_dir == NULL) {
		return 0;
	}

	dir_entry = (struct dirent *) malloc( 
		offsetof(struct dirent, d_name) + 256);

	while ((readdir_r(block_dir, dir_entry, &de) == 0) && 
		de != NULL && (diskcount < MAX_DISKS)) {


		/* if it starts with hd or sd it's probably a disk */
		if ((strncmp(dir_entry->d_name, "hd", 2) == 0) ||
		    (strncmp(dir_entry->d_name, "sd", 2) == 0)) {

			sprintf(filename, "%s/%s/removable", BLOCK_DEV_PATH, 
				dir_entry->d_name);
			
			/* if we can't read the data we skip to the next one */
			if (read_string_from_file(filename, buf)) {
				if (buf[0] == '1') {
					continue;
				}

			}
			sprintf(filename, "%s/%s/size", BLOCK_DEV_PATH, 
				dir_entry->d_name);
			if (! read_string_from_file(filename, buf)) {
				continue;
			}
			disk_size = (double) atoll(buf) / 2097152;

			sprintf(disk_info[diskcount].device, "%s", 
				dir_entry->d_name);

			disk_info[diskcount].size = disk_size;
			diskcount ++;
		}
	}

	closedir(block_dir);
	free(dir_entry);
	disk_qty = diskcount;
}

/***********************************************************
 *  figure out the running time (hr, min, secs)
 ***********************************************************/
int running_time(char *buf) {

	int run_time;
	int hours;
	int mins;
	int secs;

	run_time = time(NULL) - start_time;

	hours = run_time / 3600;
	run_time = run_time % 3600;
	mins = run_time / 60;
	secs = run_time % 60;

	sprintf(buf, "%02dh %02dm %02ds", hours, mins, secs);
}

/***********************************************************
 *  Draw a graph when handed a percentage                  *
 ***********************************************************/
int draw_graph(WINDOW *win, int y, char *label, double percent) {

	int bar_width, show_bar;

	bar_width = COLS - 21;
	
	show_bar = percent * ((double) bar_width / 100);
	mvwprintw(win, y, 1, label, show_bar);

	wattron(win, COLOR_PAIR(2));
	wattron(win, A_BOLD);
	mvwprintw(win, y, 13, "|");
	mvwhline(win, y, 14, '=', show_bar);
	mvwprintw(win, y, bar_width + 14, "|");
	wattroff(win, COLOR_PAIR(2));
	wattroff(win, A_BOLD);

	mvwprintw(win, y, bar_width + 16 , "%3.0f%%", percent);
}

int draw_detail_window(WINDOW *win) {

	int i = 0;

	werase(win);

	mvwprintw(win, 0, 0, "Hardware details\n");
	
	mvwhline(win, 1, 0, 0, COLS); 

	mvwprintw(win, 2, 0, "Mobo : %s - %s\n", 
		dmi_data.board_vendor, dmi_data.board_product);

	wprintw(win, "BIOS : %s\n", dmi_data.bios_version);

	wprintw(win, "CPU  : %d x %s (%.0f MHz)\n", cpu_info.qty, 
		cpu_info.model, cpu_info.speed);

	wprintw(win, "RAM  : %.0f%% - %.1f MB / %.02f MB", 
		mem_info.mem_percent, (double) 
		(mem_info.mem_total - mem_info.mem_free) / 1024, 
		(double) mem_info.mem_total / 1024);

	if (benchmarks.completed) {
		wprintw(win, " (%.2f MB/s)", benchmarks.stream.triad);
	}
	wprintw(win, "\n");

	for (i = 0; i < disk_qty; i ++) {

		wprintw(win, "HDD  : ");
		wprintw(win, "%s:%.1fGB ", disk_info[i].device, 
			disk_info[i].size);
		if (disk_info[i].performance > 0) {
			wprintw(win, "(%.1f MB/s) ", disk_info[i].performance); 
		}
		wprintw(win, "\n");
	}
	if (disk_qty <= 0) {
		wprintw(win, "HDD  : No hard disk drives found\n");	
	}

	for (i = 0; i < nic_qty; i ++) {
		wprintw(win, "NIC  : "); 
		wprintw(win, "%s %s (%s)\n", nic_data[i].name, 
			nic_data[i].ipaddr, nic_data[i].macaddr);
	}
	if (nic_qty <= 0) {
		wprintw(win, "NIC  : No network cards found\n");	
	}

	if (enabled_ssh) {
		mvwprintw(details_win, 16, 1, "You can SSH to any IP address assigned above with the username\n 'ssh' to see progress remotely");
	}

	mvwprintw(win, 19, 0, "Press [ESC] to close");

}


/***********************************************************
 *  Draw a the statistics windows                          *
 ***********************************************************/
int draw_stat_window(WINDOW *win) {

	char label[BUFSIZ];
	char run_time[BUFSIZ];
	int pos = 0;
	int i = 0, j =0;
	int burnin_start = 0;
	int temp_displayed = 0, fan_displayed = 0;
	int linepos = 4;

	memset(label, sizeof(label), '\0');
	memset(run_time, sizeof(run_time), '\0');

	mvwprintw(win, 0, 1, "%s Version: %s", PRODUCT_NAME, PRODUCT_VERSION);

	mvwhline(win, 1, 0, 0, COLS); 

	draw_graph(win, 2, "CPU usage", cpu_usage[0].percent);
	draw_graph(win, 3, "Mem Usage", mem_info.mem_percent);
        mvwprintw(win, 4, 1, "Temps       ");

	for (i = 0; i < temp_qty; i ++) {
		if (temp_data[i].value > 0) {
			char string[22];
			char label[15];
			
			if (strlen(temp_data[i].name) > 14) {
				for (j = 0; j < 14; j++) {
					label[j] = temp_data[i].name[j];
				}
			}
			else {
				sprintf(label, "%s", temp_data[i].name);
			}

			snprintf(string, sizeof(string), "%s: %.1fC", label, temp_data[i].value);

			for (j = strlen(string); j < 22; j ++) {
				string[j] = ' ';
			}
			string[22] = '\0';

			if (temp_data[i].value >= TEMP_RED) {
				wattron(win,COLOR_PAIR(1));
				wprintw(win, "%s ", string);
				wattroff(win,COLOR_PAIR(1));
			}
			else if (temp_data[i].value >= TEMP_YELLOW) {
				wattron(win,COLOR_PAIR(3));
				wprintw(win, "%s ", string);
				wattroff(win,COLOR_PAIR(3));
			}
			else {
				wprintw(win, "%s ", string);
			}
			temp_displayed ++;

			if (temp_displayed % 3 == 0) {
				linepos ++;
        			mvwprintw(win, linepos, 1, "            ");
			}
		}
	}
	if (temp_qty == 0) {
		wprintw(win, "Not supported");
	}

	linepos ++;
        mvwprintw(win, linepos, 1, "Fans        ", fan_qty);

	for (i = 0; i < fan_qty; i ++) {
		if (fan_data[i].value >= 0) {
			char string[22];
			char label[10];
	
			if (strlen(fan_data[i].name) > 10) {
				for (j = 0; j < 10; j++) {
					label[j] = fan_data[i].name[j];
				}
			}
			else {
				sprintf(label, "%s", fan_data[i].name);
			}
	
			snprintf(string, sizeof(string), "%s: %d RPM", label, fan_data[i].value);
	
			for (j = strlen(string); j < 22; j ++) {
				string[j] = ' ';
			}
			string[22] = '\0';
			wprintw(win, "%s ", string);
	
			fan_displayed ++;

			if (fan_displayed % 3 == 0) {
				linepos ++;
       				mvwprintw(win, linepos, 1, "            ");
			}
		}
	}

	if (fan_qty == 0) {
		wprintw(win, "Not supported");
	}

	mvwhline(win, linepos + 2, 0, 0, COLS); 

	burnin_start = (linepos + 3);

	sprintf(label, "Test");
	pos = (17 - strlen(label)) / 2;
	mvwprintw(win, burnin_start, pos, label);

	mvwprintw(win, burnin_start, 17, "Pass");
	mvwprintw(win, burnin_start, 22, "Fail");

	sprintf(label, "Last message");
	pos = COLS - 27 - strlen(label) / 2;
	mvwprintw(win, burnin_start, pos, label);

	mvwhline(win, burnin_start + 1, 0, 0, COLS); 
	mvwhline(win, burnin_start + burnin_script_count + 2, 0, 0, COLS); 

	/* mvwvline(win, burnin_start + 2, 16, 0, burnin_script_count + 1);
	mvwvline(win, burnin_start + 2, 21, 0, burnin_script_count + 1);
	mvwvline(win, burnin_start + 2, 26, 0, burnin_script_count + 1); */


	for (i = 0; i < burnin_script_count; i ++) {
		pos = burnin_start + 2 + i;
		int string_width = COLS - 27 - 3;
		char buf[BUFSIZ];

		if (burnin_scripts[i].running == 1) {
			wattron(win, COLOR_PAIR(2));
			mvwprintw(win, pos, 1, "%s", burnin_scripts[i].name);
			wattroff(win, COLOR_PAIR(2));
		}
		else {
			mvwprintw(win, pos, 1, "%s", burnin_scripts[i].name);
		}
		mvwvline(win, pos, 16, 0, 1);
		mvwvline(win, pos, 21, 0, 1);
		mvwvline(win, pos, 26, 0, 1);

		mvwprintw(win, pos, 17, "%4d", burnin_scripts[i].pass_count);

		if (burnin_scripts[i].fail_count > 0) {
			wattron(win, COLOR_PAIR(1));
			mvwprintw(win, pos, 22, "%4d", burnin_scripts[i].fail_count);
			wattroff(win, COLOR_PAIR(1));
		}
		else {
			mvwprintw(win, pos, 22, "%4d", burnin_scripts[i].fail_count);
		}

		sprintf(buf, "%%.%ds", string_width);
	
		mvwprintw(win, pos, 27, buf, burnin_scripts[i].last_msg);
	}

	running_time(run_time);
	mvwprintw(bottom_bar, 1, 0," [F2] = hardware info  [F3] = dump log to usb  [F8] = quit");
	wattron(bottom_bar,COLOR_PAIR(2));
	mvwprintw(bottom_bar, 1, COLS - 13, "%s", run_time);
	wattroff(bottom_bar,COLOR_PAIR(2));

}

/***********************************************************
 *  Initialize the ncurses enviornment                     *
 ***********************************************************/
int setup_screen() {

	int log_x, log_y, log_width, log_height;
	int stat_x, stat_y, stat_width, stat_height;

	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	start_color();
	noecho();
	clrtobot();

	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);

	stat_width = COLS;
	printf("%d\n", temp_qty);
	stat_height = STAT_MIN_ROWS + (temp_qty / 3) +  (fan_qty / 3) +  burnin_script_count;
	stat_y = 0;
	stat_x = 0;

	log_height = LINES - stat_height - 3;
	log_width = COLS;
	log_y = stat_height + 1;
	log_x = 0;


	main_win = create_newwin(LINES, COLS, 0, 0);

	log_win = create_subwin(main_win, log_height, log_width, log_y, log_x);
	stat_win = create_subwin(main_win, stat_height, stat_width, stat_y, stat_x);
	bottom_bar = create_subwin(main_win, 2, COLS, LINES-2, 0);

	mvwhline(bottom_bar, 0, 0, 0, COLS); 

	details_border = create_newwin(22,72,1,4);
	details_win = create_subwin(details_border, 20, 68, 2, 6);
	box(details_border, 0, 0);



	details_panel = new_panel(details_border);
	hide_panel(details_panel);

	main_panel = new_panel(main_win);

	update_panels();
	doupdate();

	scrollok(log_win, TRUE);

	touchwin(stdscr);
	refresh();
}

/***********************************************************
 *  Run a series of benchmarks on various systems
 ***********************************************************/
int run_benchmarks() {

	char buf[BUFSIZ];
	double disk_perf;
	int i;

	log_message("Running memory performance benchmark");
	bench_stream(&benchmarks.stream);

#ifndef THREADING
	update_stats();
#endif

	for (i = 0; i < disk_qty; i ++) {
		sprintf(buf, "Running disk benchmark on %s", disk_info[i].device);
		log_message(buf);
		sprintf(buf, "/dev/%s", disk_info[i].device);	
		disk_info[i].performance = bench_disk(buf);
	}
	benchmarks.completed = 1;

	return 0;
}

/***********************************************************
 *  Fork off a command and return a file ptr of stdout
 ***********************************************************/
FILE *fork_cmd(int *returnpid, char *label, char *cmd) {

	int pid, pipe_flags, stdout_pipe[2];
	FILE *fp;

	/* print our label to the log window */
	if (label != NULL) {
		log_message(label);
	}

	pipe(stdout_pipe);

	pid = fork();

	/* we are the child */
	if (pid == 0) {

		close(2);  /* we really don't want stderr */
		close(1);  /* we don't want stdtout to print */

		dup2(stdout_pipe[1], 1);  /* remap stdout to our pipe */

		execve(cmd, NULL, NULL);
	}

	/* parent does not need stdout from the pipe */

	/* our pipe input -- we want to go no non-blocking */
	if (fcntl(stdout_pipe[0], F_GETFL, pipe_flags) == -1) {
		wprintw(log_win, "Error getting ! %d %s\n", errno, strerror(errno));
		wrefresh(log_win);
	}
	pipe_flags = O_NONBLOCK;

	if (fcntl(stdout_pipe[0], F_SETFL, pipe_flags) == -1) {
		return NULL;
	}
	close(stdout_pipe[1]);

	fp = fdopen(stdout_pipe[0], "r");
	if (fp == NULL) {
		wprintw(log_win, "Error %s\n", strerror(errno));
		wrefresh(log_win);
	}
	*returnpid = pid;

	return fp;
}



/***********************************************************
 *  Write the stats to a disk file
 ***********************************************************/
int dump_stats() {

	FILE *fp;
	int i = 0;

	fp = fopen(STAT_FILE, "w+");
	if (fp == NULL) {
		return 1;
	}

	fprintf(fp, "CPU_QTY=\"%d\"\n", cpu_info.qty);
	fprintf(fp, "CPU_SPEED=\"%.0f\"\n", cpu_info.speed);
	fprintf(fp, "CPU_MODEL=\"%s\"\n", cpu_info.model);
	fprintf(fp, "CPU_PERCENT=\"%0.f\"\n", cpu_usage[i].percent);

	for (i = 1; i < cpu_qty; i ++) {
		fprintf(fp, "CPU_%d_PERCENT=\"%.0f\"\n", i - 1, 
			cpu_usage[i].percent);
	}

	fprintf(fp, "MEM_TOTAL=\"%ld\"\n", mem_info.mem_total);
	fprintf(fp, "MEM_FREE=\"%ld\"\n", mem_info.mem_free);
	fprintf(fp, "MEM_PERCENT=\"%.0f\"\n", mem_info.mem_percent);

	fprintf(fp, "SWAP_TOTAL=\"%ld\"\n", mem_info.swap_total);
	fprintf(fp, "SWAP_FREE=\"%ld\"\n", mem_info.swap_free);
	fprintf(fp, "SWAP_PERCENT=\"%.0f\"\n", mem_info.swap_percent);

	fprintf(fp, "TEMP_QTY=\"%d\"\n", temp_qty);
	for (i = 0; i < temp_qty; i ++) {
		fprintf(fp, "TEMP_%d=\"%.0f\"\n", i, temp_data[i].value);
	}

	fprintf(fp, "DISK_QTY=\"%d\"\n", disk_qty);
	for (i = 0; i < disk_qty; i ++) {
		fprintf(fp, "DISK_%d_NAME=\"%s\"\n", i, disk_info[i].device);
		fprintf(fp, "DISK_%d_SIZE=\"%f\"\n", i, disk_info[i].size);
		fprintf(fp, "DISK_%d_PERFORMANCE=\"%f\"\n", i, 
			disk_info[i].performance);
	}

	fprintf(fp, "NIC_QTY=\"%d\"\n", nic_qty);
	for (i = 0; i < nic_qty; i ++) {
		fprintf(fp, "NIC_%d_NAME=\"%s\"\n", i, nic_data[i].name);
		fprintf(fp, "NIC_%d_MAC=\"%s\"\n", i, nic_data[i].macaddr);
		fprintf(fp, "NIC_%d_IPADDR=\"%s\"\n", i, nic_data[i].ipaddr);
	}

	fprintf(fp, "BIOS_VENDOR=\"%s\"\n", dmi_data.bios_vendor);
	fprintf(fp, "BIOS_VERSION=\"%s\"\n", dmi_data.bios_version);
	fprintf(fp, "BIOS_DATE=\"%s\"\n", dmi_data.bios_date);
	fprintf(fp, "SYS_VENDOR=\"%s\"\n", dmi_data.sys_vendor);
	fprintf(fp, "SYS_PRODUCT=\"%s\"\n", dmi_data.sys_product);
	fprintf(fp, "SYS_VERSION=\"%s\"\n", dmi_data.sys_version);
	fprintf(fp, "SYS_SERIAL=\"%s\"\n", dmi_data.sys_serial);
	fprintf(fp, "BOARD_VENDOR=\"%s\"\n", dmi_data.board_vendor);
	fprintf(fp, "BOARD_PRODUCT=\"%s\"\n", dmi_data.board_product);
	fprintf(fp, "BOARD_VERSION=\"%s\"\n", dmi_data.board_version);
	fprintf(fp, "BOARD_SERIAL=\"%s\"\n", dmi_data.board_serial);

	if (benchmarks.completed) {
		fprintf(fp, "MEM_TRIAD=\"%f\"\n", benchmarks.stream.triad);
		fprintf(fp, "MEM_ADD=\"%f\"\n", benchmarks.stream.add);
		fprintf(fp, "MEM_COPY=\"%f\"\n", benchmarks.stream.copy);
		fprintf(fp, "MEM_SCALE=\"%f\"\n", benchmarks.stream.scale);
	}

	fprintf(fp, "BURNIN_QTY=\"%d\"\n", burnin_script_count);
	int total_failed = 0;
	for (i = 0; i < burnin_script_count; i ++) {
		fprintf(fp, "BURNIN_%d_NAME=\"%s\"\n", i, 
			burnin_scripts[i].name);
		fprintf(fp, "BURNIN_%d_LASTMSG=\"%s\"\n", i, 
			burnin_scripts[i].last_msg);
		fprintf(fp, "BURNIN_%d_RUNNING=\"%d\"\n", i, 
			burnin_scripts[i].running);
		fprintf(fp, "BURNIN_%d_PASS_QTY=\"%d\"\n", i, 
			burnin_scripts[i].pass_count);
		fprintf(fp, "BURNIN_%d_FAIL_QTY=\"%d\"\n", i, 
			burnin_scripts[i].fail_count);
		total_failed = total_failed + burnin_scripts[i].fail_count;
	}
	fprintf(fp, "BURNIN_TOTAL_FAIL_QTY=\"%d\"\n", total_failed);

	fclose(fp);
}

/* this should be called within it's own thread */
int dump_log() {

	struct timespec ts;
	FILE *dump_fp;
	int finished = 0, pid, pidstatus;
	char curtime[BUFSIZ], cmd[BUFSIZ], buf[BUFSIZ];

	running_time(curtime);
	
	dump_fp = fork_cmd(&pid, "Dumping log to USB device", 
		"/etc/breakin/dumplog.sh"); 

	while (!finished && dump_fp != NULL) {

		memset(buf, sizeof(buf), '\0');
		while(fgets(buf, sizeof(buf), dump_fp) != NULL) {
			log_message(buf);
		} 

		if (waitpid(pid, &pidstatus, WNOHANG) != 0) {
			fclose(dump_fp);
			finished = 1;
		}
		ts.tv_sec = 0;
		ts.tv_nsec = SLEEP_DELAY;
		nanosleep(&ts, NULL);
	}
}



int handle_keypress() {
	int ch;
	struct timespec ts;

	while (ch = getch()) {
		/* F2 = hardware details */
		if (ch == KEY_F(2)) {
			show_panel(details_panel);
			top_panel(details_panel);
			update_panels();
			doupdate();
			refresh();
		}
		/* F3 = copy log to usb */
		if (ch == KEY_F(3)) {

			pthread_t dump_thread;
			pthread_create(&dump_thread, NULL , (void *)dump_log, NULL);
			pthread_detach(dump_thread);
		}
		/* F8 = quit */
		else if (ch == KEY_F(8)) {
			endwin();
			if (logdev_file != NULL) {
				fclose(logdev_file);
			}
			execv("/etc/breakin/stop.sh", NULL);
		}
		else if (ch == ESCAPE) {
			hide_panel(details_panel);
			update_panels();
			doupdate();
			refresh();
		}

		ts.tv_sec = 0;
		ts.tv_nsec = KEY_DELAY;
		nanosleep(&ts, NULL);

	}
}

/***********************************************************
 *  the systems stats function
 ***********************************************************/
int update_stats() {

	/* this is run off in a seperate thread so that it's always updating
	   the stats */
#ifdef THREADING
	while (1) {
#endif
		struct timespec ts;

		monitor_syslog();

		get_cpu_usage(); 
		get_mem_usage();
		get_ipmi_sensors(); 
		get_hd_info(); 
		get_nic_info();

		show_log(log_win);
		draw_stat_window(stat_win);
		draw_detail_window(details_win);
		dump_stats(); 

		/*wrefresh(stat_win);
		wrefresh(log_win);

		wrefresh(details_border);*/


		update_panels();
		/*doupdate();*/

		touchwin(main_win);
		touchwin(details_border);

		refresh();
#ifdef THREADING
		ts.tv_sec = 0;
		ts.tv_nsec = THREAD_DELAY;
		nanosleep(&ts, NULL);
	}
#endif

	return 0;
}

int find_burnin_tests() {

	DIR *burnin_dir;
	struct dirent *dir_entry, *de;
	char filename[BUFSIZ];
	int burnin_count = 0;
	char *strptr, *str, *token;
	int j;

	dir_entry = (struct dirent *) malloc( 
		offsetof(struct dirent, d_name) + 256);

	burnin_dir = opendir(BURNIN_SCRIPT_PATH);
	if(burnin_dir == NULL) {
		return 0;
	}


	while ((readdir_r(burnin_dir, dir_entry, &de) == 0) && de != NULL) {

		int disable_test = 0;

		for (j = 1, str = breakin_disable; ;j++, str = NULL) {
			token = strtok_r(str, ",", &strptr);
			if (token == NULL) {
				break;
			}

			/* we got ourselves a match disable this test */
			if (strcmp(token, dir_entry->d_name) == 0) {
				disable_test = 1;
				break;
			}
		}

		if (disable_test) {
			continue;
		}

		if (dir_entry->d_name[0] != '.') {

			sprintf(filename, "%s/%s", BURNIN_SCRIPT_PATH, dir_entry->d_name);
			sprintf(burnin_scripts[burnin_count].name, dir_entry->d_name);
			sprintf(burnin_scripts[burnin_count].path, filename);
			burnin_scripts[burnin_count].pass_count = 0;
			burnin_scripts[burnin_count].fail_count = 0;
			burnin_scripts[burnin_count].running = 0;
			burnin_scripts[burnin_count].last_result = 0;

			memset(burnin_scripts[burnin_count].last_msg, 
				sizeof(burnin_scripts[burnin_count].last_msg), 
				'\0');
			burnin_count ++;
		}
	}
	closedir(burnin_dir);
	free(dir_entry);
	burnin_script_count = burnin_count;
	return burnin_count;
}

int run_burnin_tests() {

	DIR *burnin_dir;
	struct dirent *dir_entry, *de;
	char label[BUFSIZ];
	int i = 0;

	dir_entry = (struct dirent *) malloc( 
		offsetof(struct dirent, d_name) + 256);

	for (i = 0; i < burnin_script_count; i ++) {
		if (burnin_scripts[i].running == 0) {
			burnin_scripts[i].fp_stdout = fork_cmd(&burnin_scripts[i].pid, NULL, burnin_scripts[i].path); 

			burnin_scripts[i].running = 1;
		}
	}
}

int start_serial() {
	struct termios options;
	char buf[BUFSIZ];

	serial_fd = open(serial_dev, O_RDWR | O_NOCTTY);
	if (serial_fd == -1) {
		sprintf(buf, "Error opening serial port %s", serial_dev);
		log_error(buf);
		serial_enabled = 0;
		return 1;
	}
	fcntl(serial_fd, F_SETFL, 0);
	
	tcgetattr(serial_fd, &options);
	switch (serial_baud) {
		case 115200:
			cfsetispeed(&options, B115200);
			cfsetospeed(&options, B115200);
			break;
		case 57600:
			cfsetispeed(&options, B57600);
			cfsetospeed(&options, B57600);
			break;
		case 38400:
			cfsetispeed(&options, B38400);
			cfsetospeed(&options, B38400);
			break;
		case 19200:
			cfsetispeed(&options, B19200);
			cfsetospeed(&options, B19200);
			break;
		default:
			cfsetispeed(&options, B9600);
			cfsetospeed(&options, B9600);
			break;
	}

	/* set port to 8N1 */
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	
	tcsetattr(serial_fd, TCSANOW, &options);


	serial_file = fdopen(serial_fd, "w+");
	if (serial_file == NULL) {
		printf("Error :%d\n", errno);
		exit;
	}

}

int header_logdev(FILE *fp) {
	int i = 0;

	fprintf(fp, "=================================================================\n");
	fprintf(fp, "  New breakin test started\n");
	fprintf(fp, "=================================================================\n");

	fprintf(fp, "");

	fprintf(fp, "Mobo : %s - %s\n", 
		dmi_data.board_vendor, dmi_data.board_product);

	fprintf(fp, "BIOS : %s\n", dmi_data.bios_version);

	fprintf(fp, "CPU  : %d x %s (%.0f MHz)\n", cpu_info.qty, 
		cpu_info.model, cpu_info.speed);

	fprintf(fp, "RAM  : %.0f%% - %.1f MB / %.02f MB", 
		mem_info.mem_percent, (double) 
		(mem_info.mem_total - mem_info.mem_free) / 1024, 
		(double) mem_info.mem_total / 1024);

	if (benchmarks.completed) {
		fprintf(fp, " (%.2f MB/s)", benchmarks.stream.triad);
	}
	fprintf(fp, "\n");

	for (i = 0; i < disk_qty; i ++) {

		fprintf(fp, "HDD  : ");
		fprintf(fp, "%s:%.1fGB ", disk_info[i].device, 
			disk_info[i].size);
		if (disk_info[i].performance > 0) {
			fprintf(fp, "(%.1f MB/s) ", disk_info[i].performance); 
		}
		fprintf(fp, "\n");
	}
	if (disk_qty <= 0) {
		fprintf(fp, "HDD  : No hard disk drives found\n");	
	}

	for (i = 0; i < nic_qty; i ++) {
		fprintf(fp, "NIC  : "); 
		fprintf(fp, "%s %s (%s)\n", nic_data[i].name, 
			nic_data[i].ipaddr, nic_data[i].macaddr);
	}
	if (nic_qty <= 0) {
		fprintf(fp, "NIC  : No network cards found\n");	
	}
	fflush(fp);

	fprintf(fp, "\n");

}


int start_logdev() {

	char buf[BUFSIZ];

	logdev_file = fopen(LOG_DEV_FILE, "w+");
	if (logdev_file == NULL) {
		return 1;
	}

	if (serial_enabled) {
		log_message("Starting serial port logging");
		start_serial(); 
	}

	header_logdev(logdev_file);

	if (serial_enabled) {
		header_logdev(serial_file);
	}
}

int update_logdev_write(FILE *fp) {

	char cur_time[BUFSIZ];
	int i = 0;

	if (fp == NULL) {
		return 0;
	}

	running_time(cur_time);
	
	fprintf(fp, "%s : temps:", cur_time);
	for (i = 0; i < temp_qty; i ++) {
		if (temp_data[i].value > 0) {
			fprintf(fp, "%.1fC  ", temp_data[i].value);
		}
	}
	if (temp_qty == 0) {
		fprintf(fp, "Not supported");
	}
	fprintf(fp, "\n");

	for (i = 0; i < burnin_script_count; i ++) {
		fprintf(fp, "%s : ", cur_time);
		fprintf(fp, "test:%s ", burnin_scripts[i].name);
		fprintf(fp, " pass:%d fail:%d", burnin_scripts[i].pass_count, burnin_scripts[i].fail_count);
		fprintf(fp, " lastmsg:%s", 
			burnin_scripts[i].last_msg);
		fprintf(fp, "\n");
	}
	fflush(fp);
}

int update_logdev() {

	struct timespec ts;

	while (1) {

		update_logdev_write(logdev_file);

		if (serial_enabled) {
			update_logdev_write(serial_file);	
		}


		ts.tv_sec = LOGDEV_DELAY;
		ts.tv_nsec = 0;
		nanosleep(&ts, NULL);
	}

}

/***********************************************************
 *  the main loop
 ***********************************************************/
int main(int argc, char **argv) {

	FILE *hw_fp;
	char buf[BUFSIZ];
	int pid, pidstatus;
	int state = 0;
	int uid;
	int done = 0;
	char cur_time[BUFSIZ];
	struct timespec ts;

#ifdef THREADING
	pthread_t update_thread, upload_thread, keypress_thread, logdev_thread;
#endif

	/* ignore the alarm signal */
	signal(SIGALRM, SIG_IGN);

	/* the benchmarks have not been completed on startup */
	benchmarks.completed = 0;

	/* program must be run as root */
	if (getuid() != 0) {
		printf("Must be run as root\n");
		exit(255);
	}

	while (1) {
		int c;
		int option_index;

		static struct option long_options[] = {
			{ "update_url", 1, 0, 'u' },
			{ "update_interval", 1, 0, 'i' },
			{ "nobenchmark", 0, 0, 'b' },
			{ "ssh", 0, 0, 's' },
			{ "baud", 1, 0, 'r' },
			{ "serialdev", 1, 0, 'd' },
			{ "disable_test", 1, 0, 't' },
			{ 0, 0, 0, 0 }
		};


		c = getopt_long(argc, argv, "u:i:bsd:r:t:", long_options, &option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'u':
				enable_updates = 1;
				snprintf(update_url, sizeof(update_url), "%s", optarg);
				break;
			case 'i':
				update_interval = atoi(optarg);
				if (update_interval < 10) {
					update_interval = DEFAULT_UPDATE_INTERVAL;
				}
				break;

			case 'b':
				enable_benchmarks = 0;
				break;

			case 's':
				enabled_ssh = 1;
				break;

			case 'r':
				serial_baud = atoi(optarg);
				break;

			case 'd':
				snprintf(serial_dev, sizeof(serial_dev), "%s", optarg);
				serial_enabled = 1;
				break;

			case 't':
				snprintf(breakin_disable, sizeof(breakin_disable), "%s", optarg);
				break;

			default:
				printf("%s\n", PRODUCT_NAME);
				printf("\nValid Options:\n");
				printf("  -u, --update_url=URL             The valid URL to send update data\n");
				printf("  -i, --update_interval=SECS       Send update every SECS seconds\n");
				printf("  -b, --nobenchmarks               Skip the benchmark phase\n");
				printf("  -r, --baud                       Baud rate for serial log\n");
				printf("  -d, --serialdev                  Serial port device\n");
				printf("  -t, --disable_test=test1,test2   Comma seperated lists of tests to not run\n");
				printf("\n");
				exit(1);
		}
	}

	find_burnin_tests();

	get_ipmi_sensors(); 

	setup_screen();			/* initalize the ncurses screen */

	/* these functions do not need to be run multiple times */
	do_dmi_decode();		/* get CPU information */
	get_cpu_info();			/* get CPU information */

	start_time = time(NULL); 	/* get the time we started this mess */

	/* spawn off a thread to handle the screen updates and 
	   do stats collection on hardware */

	/* file to output all messages to */
	log_file = fopen(LOG_FILE, "a");
	if (log_file == NULL) {
		printf("Can't open log file: %s\n", LOG_FILE);
	}

#ifdef THREADING
	pthread_create(&update_thread, NULL , (void *)update_stats, NULL);
	pthread_detach(update_thread);

	if (enable_updates) {
		pthread_create(&upload_thread, NULL , (void *)do_update, NULL);
		pthread_detach(upload_thread);
	}

	pthread_create(&keypress_thread, NULL, (void *)handle_keypress, NULL);
	pthread_detach(keypress_thread);

#endif

	ts.tv_sec = 2;
	ts.tv_nsec = 0;
	nanosleep(&ts, NULL);

	/* step 1: we setup the hardware */
	hw_fp = fork_cmd(&pid, "Staring hardware setup", 
		"/etc/breakin/hardware.sh"); 

	hardware_finished = 0;	/* our hardware setup is not done */

	while (!hardware_finished && hw_fp != NULL) {

		/* redraw the screen */
#ifndef THREADING
		update_stats();
#endif

		memset(buf, sizeof(buf), '\0');
		while(fgets(buf, sizeof(buf), hw_fp) != NULL) {
			wprintw(log_win, "%s", buf);
		} 

		if (waitpid(pid, &pidstatus, WNOHANG) != 0) {
			fclose(hw_fp);
			log_message("Finished hardware setup");
			hardware_finished = 1;
		}
		ts.tv_sec = 0;
		ts.tv_nsec = SLEEP_DELAY;
		nanosleep(&ts, NULL);
	}

	if (strcmp("", breakin_disable) != 0) {
		sprintf(buf, "Was asked to disable the following tests: %s", breakin_disable);
		log_message(buf);
	}



	/* hardware setup is finished ... now it's time to try to start logging */

	 start_logdev(); 

#ifdef THREADING
	if (logdev_file != NULL) {
		pthread_create(&logdev_thread, NULL , (void *)update_logdev, NULL);
		pthread_detach(logdev_thread);
	}
#endif

#ifdef THREADING
	if (enable_benchmarks) {
		run_benchmarks();  
		benchmarks.completed = 1;
	}
#endif 

	if (enable_updates) {
		sprintf(buf, "Sending updates to %s every %d seconds", update_url, update_interval);
		log_message(buf);
	}

	if (serial_enabled) {
		sprintf(buf, "Serial logging enabled on %s baud %d", serial_dev, serial_baud);
		log_message(buf);
	}

	if (enabled_ssh) {
		log_message("SSH server listing on port 22");
	}

	log_message("Starting burnin tests");

	/* step 3: we start the burnin tests */

	while (1) {
		int pidstatus;
		int i;
		char buf[BUFSIZ], buf2[BUFSIZ];


#ifndef THREADING
		update_stats();
#endif
		run_burnin_tests();

		 for (i = 0; i < burnin_script_count; i ++) {
			int pid;

			memset(buf, sizeof(buf), '\0');
			memset(buf2, sizeof(buf2), '\0');

			if (burnin_scripts[i].running > 0) {

				if (burnin_scripts[i].fp_stdout == NULL) {
					sprintf(buf, "Error reading stdout from: %s\n", 
						burnin_scripts[i].name);

					log_message(buf);
					continue;
				}
				while (fgets(buf, sizeof(buf), 
					burnin_scripts[i].fp_stdout) != NULL) {

					trim(buf);

					if ((strncmp(buf, "MSG: ", 5) == 0) && (strlen(buf) > 5)) {
						char *ptr;

						ptr = buf;
						ptr += 5;
						snprintf(burnin_scripts[i].last_msg, 
							sizeof(burnin_scripts[i].last_msg), ptr);
					}
					else {
						snprintf(burnin_scripts[i].last_msg, 
							sizeof(burnin_scripts[i].last_msg), buf);
						sprintf(buf2, "%s - %s", burnin_scripts[i].name, buf);
						log_error(buf2);
					}
				}

				pid = waitpid(burnin_scripts[i].pid, &pidstatus, WNOHANG);
				if (pid == -1) {
					burnin_scripts[i].running = 0;
					burnin_scripts[i].last_result = 0;
					burnin_scripts[i].pass_count ++;
					burnin_scripts[i].running = 0;	
				}
				if (pid > 0) {

					if (burnin_scripts[i].fp_stdout != NULL) {
						fclose(burnin_scripts[i].fp_stdout);
					}

					if (! WIFEXITED(pidstatus)) {
						sprintf(buf, "Execution failed on '%s', disabling.", 
							burnin_scripts[i].name);
						log_message(buf);
						burnin_scripts[i].running = -1;
					}
					else if (WEXITSTATUS(pidstatus) == 255) {
						sprintf(buf, "Disabling burnin test '%s'", 
							burnin_scripts[i].name);
						log_message(buf);
						burnin_scripts[i].running = -1;
						burnin_scripts[i].last_result = 0;
						burnin_scripts[i].pass_count ++;
					}
					else if (WEXITSTATUS(pidstatus) == 0) {
						burnin_scripts[i].running = 0;
						burnin_scripts[i].last_result = 0;
						burnin_scripts[i].pass_count ++;
					}
					else {
						burnin_scripts[i].running = 0;
						burnin_scripts[i].last_result = 1;
						burnin_scripts[i].fail_count ++;
					}
				}
			}
		}


		ts.tv_sec = 0;
		ts.tv_nsec = SLEEP_DELAY;
		nanosleep(&ts, NULL);
	}

	endwin();
	return 0;
}

WINDOW *create_newwin(int height, int width, int starty, int startx) {	

	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);

	wrefresh(local_win);
	return local_win;

}


WINDOW *create_subwin(WINDOW *parent, int height, int width, int starty, int startx) {	

	WINDOW *local_win;

	local_win = subwin(parent, height, width, starty, startx);

	wrefresh(local_win);
	return local_win;
}

/* 
 vim:ts=4:sw=4 
*/
