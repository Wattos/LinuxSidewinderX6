/*
    Sidewinder. This is a user space application which enables the macro
    keys of the Microsoft Sidewinder X6 keyboard.

    Copyright (C) 2011 Filip Wieladek

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include <libusb-1.0/libusb.h>

#define SIDEWINDER_PROGRAM_NAME "sidewinder-x6-macro-keys"
#define SIDEWINDER_VERSION "v0.1"

//Default username to run for
#define SIDEWINDER_FALLBACK_USER "root"

//Profile configuration constants
#define SIDEWINDER_SUDO_USER_ENV_VAR "SUDO_USER"
#define SIDEWINDER_USER_ENV_VAR "USER"
#define SIDEWINDER_PROFILE_ROOT_FOLDER ".sidewinderx6"
#define SIDEWINDER_MAX_PROFILE_COUNT 3
#define SIDEWINDER_PROFILE_FOLDER_FORMAT "p%d"
#define SIDEWINDER_PROFILE_CONFIG "macro_numpad"
#define SIDEWINDER_MACRO_FORMAT "S%d.sh"
#define SIDEWINDER_MACRO_LOAD   "load.sh"
#define SIDEWINDER_COMMAND_FORMAT "su %s -c '%s &'"

//Virutal keyboard constants
#define SIDEWINDER_UINPUT "/dev/uinput"

//Usb constants
#define SIDEWINDER_USB_MACRO_KEYS_INTERFACE 1
#define SIDEWINDER_USB_MACRO_KEYS_END_POINT 0x82
#define SIDEWINDER_VID 0x045e
#define SIDEWINDER_PID 0x074b

//Sidewinder protocol constants
#define SIDEWINDER_MEDIA_KEY_EVENT 0x01
#define SIDEWINDER_MACRO_KEY_EVENT 0x08

#define SIDEWINDER_MAX_PATH_SIZE 512

//Exit error codes
#define SIDEWINDER_EXIT_FAILURE 1
#define SIDEWINDER_EXIT_SUCCESS 0

/* Prototypes */
void sidewinder_parse_arguments(int argc, char** argv);
void sidewinder_single_instance();
void sidewinder_setup_daemon();
void sidewinder_initialize_signals();
void sidewinder_initialize();
void sidewinder_initialize_usb();
void sidewinder_initialize_virtual_keyboard();
void sidewinder_find_keyboard();
void sidewinder_run_macro(uint8_t macro);
void sidewinder_set_profile(uint8_t profile);
uint8_t sidewinder_profile_has_macropad();
void sidewinder_handle_keypress();
void sidewinder_signal_handler(int signal);
void sidewinder_run();
void sidewinder_cleanup();


/* The folders and files for the macro profiles */
char _sidewinder_folder[SIDEWINDER_MAX_PATH_SIZE];
char _sidewinder_profile_folder[SIDEWINDER_MAX_PROFILE_COUNT][SIDEWINDER_MAX_PATH_SIZE];
char _sidewinder_profile_config[SIDEWINDER_MAX_PROFILE_COUNT][SIDEWINDER_MAX_PATH_SIZE];
char _sidewinder_profile_load[SIDEWINDER_MAX_PROFILE_COUNT][SIDEWINDER_MAX_PATH_SIZE];

/* The current profile of the keyboard */
int16_t _sidewinder_current_profile = -1;

/* Usb variables */
libusb_context* 	  _sidewinder_usb_context = NULL;
libusb_device_handle* _sidewinder_keyboard_handle = NULL;

/* virtual keyboard file. Required for media buttons like volume control */
int32_t _sidewinder_virtual_keyboard_file;

/* The last press. Used to determine release events */
uint64_t _sidewinder_lastpress = 0;

/* variable used to terminate the application */
volatile uint8_t _sidewinder_run = 1;

/* Arguments */
int _sidewinder_run_as_daemon = 1;
char _sidewinder_user_name[SIDEWINDER_MAX_PATH_SIZE];
__uid_t _sidewinder_user_id = 0;
char _sidewinder_user_home[SIDEWINDER_MAX_PATH_SIZE];

int main(int argc, char** argv){
	openlog(SIDEWINDER_PROGRAM_NAME, LOG_PID|LOG_CONS, LOG_USER);	
	syslog(LOG_INFO, "%s starting", SIDEWINDER_PROGRAM_NAME);
	sidewinder_parse_arguments(argc, argv);
	sidewinder_single_instance();
	sidewinder_initialize_signals();
    
	if(_sidewinder_run_as_daemon){
		sidewinder_setup_daemon();		
	}

	sidewinder_initialize_usb();
	sidewinder_run();
	sidewinder_cleanup();

	exit(SIDEWINDER_EXIT_SUCCESS);	
}

void sidewinder_parse_arguments(int argc, char** argv) {
	static struct option long_options[] = {
      {"help",        no_argument, 0, 'h'},
      {"version",     no_argument, 0, 'v'},
      {"foreground",  no_argument, &_sidewinder_run_as_daemon, 0},
      {"user",  required_argument, 0, 'u'},
      {0, 0, 0, 0}
	};
	int c;
	int index;
	_sidewinder_user_name[0] = '\0';
	while ((c = getopt_long(argc, argv, "vhfu:",  long_options, &index)) != -1)
	{
		switch(c){
			case 'u':
				strncpy(_sidewinder_user_name, optarg, sizeof(_sidewinder_user_name));
				break;
			case 'f':
				_sidewinder_run_as_daemon = 0;
				break;
			case 'v':
				printf("%s at version: %s\n", SIDEWINDER_PROGRAM_NAME, SIDEWINDER_VERSION);
				printf("\n%s  Copyright (C) %d  %s\n", SIDEWINDER_PROGRAM_NAME, 2011, "Filip Wieladek");
				printf("This program comes with ABSOLUTELY NO WARRANTY\n");
    				printf("This is free software, and you are welcome to redistribute it\n\n");
				exit(SIDEWINDER_EXIT_SUCCESS);				
				break;
			case 'h':
			default:
				fprintf(stderr, "Usage: %s [-h] [-u <user>] [-f]\n", SIDEWINDER_PROGRAM_NAME);
				fprintf(stderr, "\n");
				fprintf(stderr, "Information Arguments: \n");
				fprintf(stderr, "   -h, --help                prints this usage dialog\n");
				fprintf(stderr, "   -v, --version             prints the version of the program\n");
				fprintf(stderr, "\nUsage Arguments: \n");
				fprintf(stderr, "   -u <user>, --user <user>  Uses the user defined in <user> for running the macros\n");
				fprintf(stderr, "   -f, --foreground          Runs in the foreground instead of running as a daemon\n");
				fprintf(stderr, "\n%s  Copyright (C) %d  %s\n", SIDEWINDER_PROGRAM_NAME, 2011, "Filip Wieladek");
				fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY\n");
    				fprintf(stderr, "This is free software, and you are welcome to redistribute it\n\n");
				exit(SIDEWINDER_EXIT_FAILURE);
		}
	}

	if(_sidewinder_user_name[0] == '\0'){
		strncpy(_sidewinder_user_name, getenv(SIDEWINDER_SUDO_USER_ENV_VAR), sizeof(_sidewinder_user_name));
	}
	if(_sidewinder_user_name[0] == '\0'){
		strncpy(_sidewinder_user_name, getenv(SIDEWINDER_USER_ENV_VAR), sizeof(_sidewinder_user_name));
	}
	if(_sidewinder_user_name[0] == '\0'){
		strncpy(_sidewinder_user_name, SIDEWINDER_FALLBACK_USER, sizeof(_sidewinder_user_name));
	}

	int bufsize = 512;
	char buf[bufsize];
	struct passwd pwd;
	struct passwd* result;

	getpwnam_r(_sidewinder_user_name, &pwd, buf, bufsize, &result);
 	_sidewinder_user_id = pwd.pw_uid;
 	strncpy(_sidewinder_user_home, pwd.pw_dir, sizeof(_sidewinder_user_home));
 	syslog(LOG_INFO, "Running for username: %s, with pid: %d", _sidewinder_user_name, _sidewinder_user_id);
}

void sidewinder_single_instance(){
	int pid_file = open("/var/run/sidewinder-x6.pid",  O_WRONLY|O_CREAT, 0666);
	int rc = flock(pid_file, LOCK_EX | LOCK_NB);
	if(rc) {
		fprintf(stderr, "Could not start. The program is either not running with super user priviliges or another instance is already running\n");
		syslog(LOG_ERR, "Could not start. The program is either not running with super user priviliges or another instance is already running");
		exit(SIDEWINDER_EXIT_FAILURE);
	}
}

void sidewinder_initialize_signals(){
	struct sigaction act;
	memset (&act, '\0', sizeof(act));
	act.sa_handler = &sidewinder_signal_handler;
	if (sigaction(SIGTERM, &act, NULL) < 0){
		syslog(LOG_ERR, "Could not initialize Signal handler SIGTERM");
	}
	if (sigaction(SIGINT, &act, NULL) < 0){
		syslog(LOG_ERR, "Could not initialize Signal handler SIGKILL");
	}
}

/* Initializes the sidewinder daemon. */
void sidewinder_setup_daemon(){
	/* Our process ID and Session ID */
	pid_t pid, sid;

	syslog(LOG_INFO, "Starting as daemon");

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0){
		syslog(LOG_ERR, "Failed to create daemon process (fork)");
		exit(SIDEWINDER_EXIT_FAILURE);
	}
	/* If we got a good PID, then
	we can exit the parent process. */
	if (pid > 0) 
		exit(SIDEWINDER_EXIT_SUCCESS);				
	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		/* Log the failure */
		exit(SIDEWINDER_EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

/* Initializes the siderwinder program */
void sidewinder_initialize(){
	uint8_t i = 0;

	/* Create all folders. Note, that these folder will not be created if they already exist */
	snprintf(_sidewinder_folder, SIDEWINDER_MAX_PATH_SIZE, "%s/%s", _sidewinder_user_home, SIDEWINDER_PROFILE_ROOT_FOLDER);

	syslog(LOG_INFO, "Using folder %s for configuration data", _sidewinder_folder);	
	mkdir(_sidewinder_folder, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	chown(_sidewinder_folder, _sidewinder_user_id, -1);

	for(i = 0; i < SIDEWINDER_MAX_PROFILE_COUNT; ++i){
		char profile_folder[SIDEWINDER_MAX_PATH_SIZE];
		snprintf(profile_folder, SIDEWINDER_MAX_PATH_SIZE, SIDEWINDER_PROFILE_FOLDER_FORMAT, i + 1);
		snprintf(_sidewinder_profile_folder[i], SIDEWINDER_MAX_PATH_SIZE, "%s/%s", _sidewinder_folder, profile_folder);
		snprintf(_sidewinder_profile_config[i], SIDEWINDER_MAX_PATH_SIZE, "%s/%s", _sidewinder_profile_folder[i], SIDEWINDER_PROFILE_CONFIG);
		snprintf(_sidewinder_profile_load[i], SIDEWINDER_MAX_PATH_SIZE, "%s/%s", _sidewinder_profile_folder[i], SIDEWINDER_MACRO_LOAD);		

		syslog(LOG_INFO, "Using folder %s for profile data for profile %d", _sidewinder_profile_folder[i], i+1);
		syslog(LOG_INFO, "Using file %s for profile configuration for profile %d", _sidewinder_profile_config[i], i+1);
		syslog(LOG_INFO, "Using file %s for profile on load script for profile %d", _sidewinder_profile_load[i], i+1);
		
		mkdir(_sidewinder_profile_folder[i], S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		chown(_sidewinder_profile_folder[i], _sidewinder_user_id, -1);
	}

	setuid(0);
	sidewinder_initialize_virtual_keyboard();

	/* Initialize virtual keyboard */
	sidewinder_find_keyboard();

	/* current profile is always 0 at start up. */
	/* TODO: Save the current state when it changes, so it is restored on restart */
	sidewinder_set_profile(0);
}

void sidewinder_initialize_usb(){
	int32_t r;
	r = libusb_init(&_sidewinder_usb_context); //initialize a library session
	if(r < 0) {
		syslog(LOG_ERR, "Could not initialize usb. Killing daemon");
		exit(SIDEWINDER_EXIT_FAILURE);
	}
#ifdef DEBUG
	libusb_set_debug(_sidewinder_usb_context, 3); //set verbosity level to 3, as suggested in the documentation
#endif
}

void sidewinder_find_keyboard(){
	if(_sidewinder_keyboard_handle != NULL){
		libusb_close(_sidewinder_keyboard_handle);
		libusb_release_interface(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_INTERFACE);
		libusb_attach_kernel_driver(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_INTERFACE);
		_sidewinder_keyboard_handle = NULL;
	}
	do {
		_sidewinder_keyboard_handle = libusb_open_device_with_vid_pid(_sidewinder_usb_context, SIDEWINDER_VID, SIDEWINDER_PID);
		if(_sidewinder_keyboard_handle <= 0){
			syslog(LOG_INFO, "Could not find the keyboard. Will retry");
			sleep(5);
		}
		else {
			if(_sidewinder_current_profile < 0){
				libusb_detach_kernel_driver(_sidewinder_keyboard_handle, 0);
				libusb_reset_device(_sidewinder_keyboard_handle);
				libusb_attach_kernel_driver(_sidewinder_keyboard_handle, 0);
				sidewinder_set_profile(0);				
			}			
			_sidewinder_lastpress = 0;
			break;
		}
	} while(_sidewinder_keyboard_handle <= 0);
	libusb_detach_kernel_driver(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_INTERFACE);
	libusb_claim_interface(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_INTERFACE);
	sidewinder_set_profile(_sidewinder_current_profile);
}

void sidewinder_run_macro(uint8_t macro){
	char macro_name[SIDEWINDER_MAX_PATH_SIZE];
	char macro_full_path[SIDEWINDER_MAX_PATH_SIZE];
	struct stat st;

	syslog(LOG_INFO, "Running macro %d", macro);

	snprintf(macro_name, SIDEWINDER_MAX_PATH_SIZE, SIDEWINDER_MACRO_FORMAT, macro);
	snprintf(macro_full_path, SIDEWINDER_MAX_PATH_SIZE, "%s/%s", _sidewinder_profile_folder[_sidewinder_current_profile], macro_name);

	syslog(LOG_DEBUG, "Checking if %s exists", macro_full_path);	
	if(stat(macro_full_path, &st) == 0){
		syslog(LOG_INFO, "Executing %s as %s", macro_full_path, _sidewinder_user_name);
		
		char command[1000];
		snprintf(command, sizeof(command), SIDEWINDER_COMMAND_FORMAT, _sidewinder_user_name, macro_full_path);
		system(command);
	}else {
		syslog(LOG_INFO, "%s does not exist. Will not be executed", macro_full_path);
	}
}

void sidewinder_set_profile(uint8_t profile){
	struct stat st;	
	syslog(LOG_INFO, "Setting profile to %d", profile);

	uint8_t changed = _sidewinder_current_profile != profile; 
	_sidewinder_current_profile = profile % SIDEWINDER_MAX_PROFILE_COUNT;

	uint8_t data[2];
	data[0] = 0x7;
	data[1] = 0x1 << (_sidewinder_current_profile + 2);

	if(sidewinder_profile_has_macropad())
		data[1] |= 0x1;

	libusb_control_transfer(_sidewinder_keyboard_handle,
							LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
							LIBUSB_REQUEST_SET_CONFIGURATION, 0x307, 0x1, data, sizeof(data), 0);

	char* load = _sidewinder_profile_load[_sidewinder_current_profile];					
	
	if(!changed)
		return;

	syslog(LOG_DEBUG, "Looking for %s", load);
	if(stat(load, &st) == 0){
		syslog(LOG_INFO, "Executing %s", load);
		
		char load_command[1000];
		snprintf(load_command, sizeof(load_command), SIDEWINDER_COMMAND_FORMAT, _sidewinder_user_name, load);

		system(load_command);
	}else{
		syslog(LOG_INFO, "%s does not exist. Will not be executed", load);	
	}
}

void sidewinder_initialize_virtual_keyboard(){
	_sidewinder_virtual_keyboard_file = open(SIDEWINDER_UINPUT, O_WRONLY | O_NDELAY );
	if(_sidewinder_virtual_keyboard_file <= 0){
		syslog(LOG_ERR, "Could not create virtual keyboard. Failed to open %s", SIDEWINDER_UINPUT);
		exit(SIDEWINDER_EXIT_FAILURE);
	}
	struct uinput_user_dev   uinp;
	memset(&uinp, 0, sizeof(uinp));
	strncpy(uinp.name, SIDEWINDER_PROGRAM_NAME, sizeof(SIDEWINDER_PROGRAM_NAME));
	uinp.id.version = 4;
	uinp.id.bustype = BUS_USB;

	ioctl(_sidewinder_virtual_keyboard_file, UI_SET_EVBIT, EV_KEY);

	uint16_t i;
	for (i=0; i<KEY_MAX; i++)  //I believe this is to tell UINPUT what keys we can make?
		ioctl(_sidewinder_virtual_keyboard_file, UI_SET_KEYBIT, i);

	int32_t retcode = write(_sidewinder_virtual_keyboard_file, &uinp, sizeof(uinp));
	retcode = (ioctl(_sidewinder_virtual_keyboard_file, UI_DEV_CREATE));
	syslog(LOG_DEBUG, "ioctl UI_DEV_CREATE returned %d", retcode);

	if (retcode) {
		syslog(LOG_ERR, "Error create uinput device %d", retcode);
		exit(SIDEWINDER_EXIT_FAILURE);
	}
}

void sidewinder_send_key(int32_t keycode)
{
	struct input_event       event;

	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = keycode; //nomodifiers!
	event.value = 1; //key pressed
	write(_sidewinder_virtual_keyboard_file, &event, sizeof(event));

	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = keycode;
	event.value = 0; //key released
	write(_sidewinder_virtual_keyboard_file, &event, sizeof(event));

	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_SYN;
	event.code = SYN_REPORT; //not sure what this is for? i'm guessing its some kind of sync thing?
	event.value = 0;
	write(_sidewinder_virtual_keyboard_file, &event, sizeof(event));
}

/* The main loop */
void sidewinder_run(){
	/* The Big Loop */
	sidewinder_initialize();
	while (_sidewinder_run) {
		sidewinder_handle_keypress();
		if(_sidewinder_keyboard_handle == NULL){
			sidewinder_find_keyboard();
		}
#if DEBUG
		printf("Main Loop iteration complete");
#endif
	}
}

void sidewinder_handle_macro_keypress(uint64_t press){
	uint64_t keys_changed = press ^ _sidewinder_lastpress;
	int8_t i;

	//we need to offset by 8 bits to ignore the type bit
	keys_changed = keys_changed >> 8;
	press = press >> 8;
	for(i = 0; i < 64; i++){
		 uint8_t bit = (keys_changed >> i) & 1;
		 uint8_t pressed = (press >> i) & 1;
		 uint8_t released = !pressed;

		 if(bit){
			 uint8_t macro = i + 1;
			 if(released)
				 sidewinder_run_macro(macro);
		 }
	}
}

void sidewinder_get_profile_from_keyboard(){
	uint8_t data[2];
	int32_t r;

	r = libusb_control_transfer(_sidewinder_keyboard_handle, 0xA1, 0x1, 0x307, 1, data, sizeof(data), 0);
	if(r == 2){
		uint8_t i;
		for(i =0; i < SIDEWINDER_MAX_PROFILE_COUNT; i++){
			if((data[1] >> 3) & 0x1){
				_sidewinder_current_profile = i;
				break;
			}
		}
	}
}

void sidewinder_handle_media_keypress(uint64_t press){
	//ignore the first 2 bytes, these are used for knowing if it is a media key or macro key
	press = press >> 8;
	switch(press){
		case 0x100000000000: sidewinder_run_macro(255); break;
		case 0x110000000000: /*Unmapped*/ break;
		case 0x140000000000: sidewinder_set_profile(_sidewinder_current_profile + 1); break;
		case 0xcd: sidewinder_send_key(KEY_PLAYPAUSE); break;
		case 0xb6: sidewinder_send_key(KEY_PREVIOUSSONG); break;
		case 0xb5: sidewinder_send_key(KEY_NEXTSONG); break;
		case 0xe2: sidewinder_send_key(KEY_MUTE); break;
		case 0xea: sidewinder_send_key(KEY_VOLUMEDOWN); break;
		case 0xe9: sidewinder_send_key(KEY_VOLUMEUP); break;
		case 0x192: sidewinder_send_key(KEY_CALC); break;
	}
}

uint8_t sidewinder_profile_has_macropad(){
	FILE* file = fopen(_sidewinder_profile_config[_sidewinder_current_profile], "r");
	if(file == NULL){
		return 0;
	}
	unsigned char enabled = fgetc(file);
	fclose(file);
	return enabled == '1';
}

void sidewinder_handle_keypress(){
	uint8_t data[8];
	int32_t i,actual_length;
	actual_length = 0;

	int32_t result = libusb_interrupt_transfer(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_END_POINT, data, sizeof(data),&actual_length, 0);
	syslog(LOG_INFO, "libusb_interrupt_transfer: %d, result: %d\n", actual_length, result);
	
	if(actual_length > 0 && result == 0){
		uint64_t press = 0;
		for(i = 0; i < sizeof(data); i++){
			uint64_t d = data[i];
			press |=  d << (8*i);
		}

		if(data[0] == SIDEWINDER_MACRO_KEY_EVENT){
			sidewinder_handle_macro_keypress(press);
		}else if(data[0] == SIDEWINDER_MEDIA_KEY_EVENT){
			sidewinder_handle_media_keypress(press);
		}
		_sidewinder_lastpress = press;
#ifdef DEBUG
		printf("F0 data:");
		for ( i = sizeof(data) - 1; i >= 0 ; i--)
			printf("%02x ", data[i]);
		printf("\n");
		for ( i = sizeof(data) - 1; i >= 0 ; i--)
			printf("%02x ", (uint32_t)(press >> (8*i)) & 0xFF );
		printf("\n");
#endif
	}else{
		_sidewinder_keyboard_handle = NULL;
	}
}

void sidewinder_signal_handler(int signal){
	_sidewinder_run = 0;
	libusb_reset_device(_sidewinder_keyboard_handle);
}

void sidewinder_cleanup(){
	closelog();
	if(_sidewinder_keyboard_handle > 0){
		libusb_release_interface(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_INTERFACE);
		libusb_attach_kernel_driver(_sidewinder_keyboard_handle, SIDEWINDER_USB_MACRO_KEYS_INTERFACE);
		libusb_close(_sidewinder_keyboard_handle);
	}

	if(_sidewinder_usb_context != NULL)
		libusb_exit(_sidewinder_usb_context);

	if(_sidewinder_virtual_keyboard_file > 0)
		close(_sidewinder_virtual_keyboard_file);

	_sidewinder_keyboard_handle = NULL;
	_sidewinder_usb_context = NULL;
}
