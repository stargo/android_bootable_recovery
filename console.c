/*
 * Copyright (C) 2010 Skrilax_CZ
 * Open Recovery Console
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
  
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"
#include "console.h"

#define CHAR_ERROR 0
#define CHAR_NOTHING 255
#define CHAR_SCROLL_DOWN 254
#define CHAR_SCROLL_UP 253
#define CHAR_BIG_SCROLL_DOWN 252
#define CHAR_BIG_SCROLL_UP 251

#define CHAR_KEY_UP 250
#define CHAR_KEY_LEFT 249
#define CHAR_KEY_RIGHT 248
#define CHAR_KEY_DOWN 247
#define CHAR_KEY_ALTLEFT 246
#define CHAR_KEY_ALTRIGHT 245
#define CHAR_KEY_SHIFTBS 244
#define CHAR_KEY_ALTBS 243
#define CHAR_ESCAPE 242

#define ESC 27

typedef struct
{
	char normal[KEY_MAX+1];
	char shifted[KEY_MAX+1];
	char alternate[KEY_MAX+1];
} keyboardLayout;

static keyboardLayout qwerty_layout;
static keyboardLayout qwertz_layout;
static keyboardLayout azerty_layout;

static keyboardLayout* current_layout;

static void
init_keypad_layout()
{
	//##BEGIN QWERTY INITIALIZATION
	int keycode;
	memset(qwerty_layout.normal, 0, KEY_MAX+1);
	memset(qwerty_layout.shifted, 0, KEY_MAX+1);
	memset(qwerty_layout.alternate, 0, KEY_MAX+1);	

	qwerty_layout.normal[KEY_A] = 'a';
	qwerty_layout.shifted[KEY_A] = 'A';
	qwerty_layout.alternate[KEY_A] = 'A';
	
	qwerty_layout.normal[KEY_B] = 'b';
	qwerty_layout.shifted[KEY_B] = 'B';
	qwerty_layout.alternate[KEY_B] = '+';
	
	qwerty_layout.normal[KEY_C] = 'c';
	qwerty_layout.shifted[KEY_C] = 'C';
	qwerty_layout.alternate[KEY_C] = '_';
	
	qwerty_layout.normal[KEY_D] = 'd';
	qwerty_layout.shifted[KEY_D] = 'D';
	qwerty_layout.alternate[KEY_D] = 'D';
	
	qwerty_layout.normal[KEY_E] = 'e';
	qwerty_layout.shifted[KEY_E] = 'E';
	qwerty_layout.alternate[KEY_E] = '$';
	
	qwerty_layout.normal[KEY_F] = 'f';
	qwerty_layout.shifted[KEY_F] = 'F';
	qwerty_layout.alternate[KEY_F] = '[';
	
	qwerty_layout.normal[KEY_G] = 'g';
	qwerty_layout.shifted[KEY_G] = 'G';
	qwerty_layout.alternate[KEY_G] = ']';
	
	qwerty_layout.normal[KEY_H] = 'h';
	qwerty_layout.shifted[KEY_H] = 'H';
	qwerty_layout.alternate[KEY_H] = '{';
	
	qwerty_layout.normal[KEY_I] = 'i';
	qwerty_layout.shifted[KEY_I] = 'I';
	qwerty_layout.alternate[KEY_I] = '('; 
	
	qwerty_layout.normal[KEY_J] = 'j';
	qwerty_layout.shifted[KEY_J] = 'J';
	qwerty_layout.alternate[KEY_J] = '}';
	
	qwerty_layout.normal[KEY_K] = 'k';
	qwerty_layout.shifted[KEY_K] = 'K';
	qwerty_layout.alternate[KEY_K] = '\\';
	
	qwerty_layout.normal[KEY_L] = 'l';
	qwerty_layout.shifted[KEY_L] = 'L';
	qwerty_layout.alternate[KEY_L] = '|';
	
	qwerty_layout.normal[KEY_M] = 'm';
	qwerty_layout.shifted[KEY_M] = 'M';
	qwerty_layout.alternate[KEY_M] = '\'';
	
	qwerty_layout.normal[KEY_N] = 'n';
	qwerty_layout.shifted[KEY_N] = 'N';
	qwerty_layout.alternate[KEY_N] = '\"';
	
	qwerty_layout.normal[KEY_O] = 'o';
	qwerty_layout.shifted[KEY_O] = 'O';
	qwerty_layout.alternate[KEY_O] = ')';
	
	qwerty_layout.normal[KEY_P] = 'p';
	qwerty_layout.shifted[KEY_P] = 'P';
	qwerty_layout.alternate[KEY_P] = '.';
	
	qwerty_layout.normal[KEY_Q] = 'q';
	qwerty_layout.shifted[KEY_Q] = 'Q';
	qwerty_layout.alternate[KEY_Q] = '!';
	
	qwerty_layout.normal[KEY_R] = 'r';
	qwerty_layout.shifted[KEY_R] = 'R';
	qwerty_layout.alternate[KEY_R] = '%';
	
	qwerty_layout.normal[KEY_S] = 's';
	qwerty_layout.shifted[KEY_S] = 'S';
	qwerty_layout.alternate[KEY_S] = '£';
	
	qwerty_layout.normal[KEY_T] = 't';
	qwerty_layout.shifted[KEY_T] = 'T';
	qwerty_layout.alternate[KEY_T] = '=';
	
	qwerty_layout.normal[KEY_U] = 'u';
	qwerty_layout.shifted[KEY_U] = 'U';
	qwerty_layout.alternate[KEY_U] = '*';
	
	qwerty_layout.normal[KEY_V] = 'v';
	qwerty_layout.shifted[KEY_V] = 'V';
	qwerty_layout.alternate[KEY_V] = '-';
	
	qwerty_layout.normal[KEY_W] = 'w';
	qwerty_layout.shifted[KEY_W] = 'W';
	qwerty_layout.alternate[KEY_W] = '#';
	
	qwerty_layout.normal[KEY_X] = 'x';
	qwerty_layout.shifted[KEY_X] = 'X';
	qwerty_layout.alternate[KEY_X] = '>';
	
	qwerty_layout.normal[KEY_Y] = 'y';
	qwerty_layout.shifted[KEY_Y] = 'Y';
	qwerty_layout.alternate[KEY_Y] = '&';
	
	qwerty_layout.normal[KEY_Z] = 'z';
	qwerty_layout.shifted[KEY_Z] = 'Z';
	qwerty_layout.alternate[KEY_Z] = '<';

	qwerty_layout.normal[KEY_COMMA] = ',';
	qwerty_layout.shifted[KEY_COMMA] = ';';
	qwerty_layout.alternate[KEY_COMMA] = ';';
	
	qwerty_layout.normal[KEY_DOT] = '.';
	qwerty_layout.shifted[KEY_DOT] = ':';
	qwerty_layout.alternate[KEY_DOT] = ':';

	qwerty_layout.normal[KEY_SLASH] = '/';
	qwerty_layout.shifted[KEY_SLASH] = '?';
	qwerty_layout.alternate[KEY_SLASH] = '?';

	qwerty_layout.normal[KEY_QUESTION] = '?';
	qwerty_layout.shifted[KEY_QUESTION] = '?';
	qwerty_layout.alternate[KEY_QUESTION] = ']';
	
	qwerty_layout.normal[KEY_TAB] = '\t';
	qwerty_layout.shifted[KEY_TAB] = '\t';
	qwerty_layout.alternate[KEY_TAB] = '\t';

	qwerty_layout.normal[KEY_SPACE] = ' ';
	qwerty_layout.shifted[KEY_SPACE] = ' ';
	qwerty_layout.alternate[KEY_SPACE] = ' ';

	qwerty_layout.normal[KEY_CENTER] = '\n';
	qwerty_layout.shifted[KEY_CENTER] = '\n';
	qwerty_layout.alternate[KEY_CENTER] = '\n';
	
	qwerty_layout.normal[KEY_ENTER] = '\n';
	qwerty_layout.shifted[KEY_ENTER] = '\n';
	qwerty_layout.alternate[KEY_ENTER] = '\n';
	
	qwerty_layout.normal[KEY_BACKSPACE] = '\b';
	qwerty_layout.shifted[KEY_BACKSPACE] = CHAR_KEY_SHIFTBS;
	qwerty_layout.alternate[KEY_BACKSPACE] = CHAR_KEY_ALTBS;
	
	qwerty_layout.normal[KEY_EMAIL] = '@';
	qwerty_layout.shifted[KEY_EMAIL] = '^';
	qwerty_layout.alternate[KEY_EMAIL] = '^';

	qwerty_layout.normal[KEY_1] = '1';
	qwerty_layout.shifted[KEY_1] = '1';
	qwerty_layout.alternate[KEY_1] = '1';

	qwerty_layout.normal[KEY_2] = '2';
	qwerty_layout.shifted[KEY_2] = '2';
	qwerty_layout.alternate[KEY_2] = '2';

	qwerty_layout.normal[KEY_3] = '3';
	qwerty_layout.shifted[KEY_3] = '3';
	qwerty_layout.alternate[KEY_3] = '3';

	qwerty_layout.normal[KEY_4] = '4';
	qwerty_layout.shifted[KEY_4] = '4';
	qwerty_layout.alternate[KEY_4] = '4';

	qwerty_layout.normal[KEY_5] = '5';
	qwerty_layout.shifted[KEY_5] = '5';
	qwerty_layout.alternate[KEY_5] = '5';

	qwerty_layout.normal[KEY_6] = '6';
	qwerty_layout.shifted[KEY_6] = '6';
	qwerty_layout.alternate[KEY_6] = '6';

	qwerty_layout.normal[KEY_7] = '7';
	qwerty_layout.shifted[KEY_7] = '7';
	qwerty_layout.alternate[KEY_7] = '7';

	qwerty_layout.normal[KEY_8] = '8';
	qwerty_layout.shifted[KEY_8] = '8';
	qwerty_layout.alternate[KEY_8] = '8';

	qwerty_layout.normal[KEY_9] = '9';
	qwerty_layout.shifted[KEY_9] = '9';
	qwerty_layout.alternate[KEY_9] = '9';

	qwerty_layout.normal[KEY_0] = '0';
	qwerty_layout.shifted[KEY_0] = '0';
	qwerty_layout.alternate[KEY_0] = '0';

	qwerty_layout.normal[KEY_GRAVE] = '`';
	qwerty_layout.shifted[KEY_GRAVE] = '~';
	qwerty_layout.alternate[KEY_GRAVE] = '~';

	qwerty_layout.normal[KEY_RECORD] = CHAR_ESCAPE;
	qwerty_layout.shifted[KEY_RECORD] = CHAR_ESCAPE;
	qwerty_layout.alternate[KEY_RECORD] = CHAR_ESCAPE;		

	//the joystick has key orientation ala portrait
	qwerty_layout.normal[KEY_UP] = CHAR_KEY_LEFT; 
	qwerty_layout.shifted[KEY_UP] = CHAR_KEY_LEFT;
	qwerty_layout.alternate[KEY_UP] = '\r';

	qwerty_layout.normal[KEY_LEFT] = CHAR_KEY_DOWN;
	qwerty_layout.shifted[KEY_LEFT] = CHAR_BIG_SCROLL_DOWN;
	qwerty_layout.alternate[KEY_LEFT] = CHAR_SCROLL_DOWN;	
	
	qwerty_layout.normal[KEY_RIGHT] = CHAR_KEY_UP;
	qwerty_layout.shifted[KEY_RIGHT] = CHAR_BIG_SCROLL_UP;
	qwerty_layout.alternate[KEY_RIGHT] = CHAR_SCROLL_UP;

	qwerty_layout.normal[KEY_DOWN] = CHAR_KEY_RIGHT;
	qwerty_layout.shifted[KEY_DOWN] = CHAR_KEY_RIGHT;
	qwerty_layout.alternate[KEY_DOWN] = CHAR_KEY_ALTRIGHT;

	qwerty_layout.normal[KEY_VOLUMEDOWN] = CHAR_SCROLL_DOWN;
	qwerty_layout.shifted[KEY_VOLUMEDOWN] = CHAR_BIG_SCROLL_DOWN;
	qwerty_layout.alternate[KEY_VOLUMEDOWN] = CHAR_SCROLL_DOWN;	
	
	qwerty_layout.normal[KEY_VOLUMEUP] = CHAR_SCROLL_UP;
	qwerty_layout.shifted[KEY_VOLUMEUP] = CHAR_BIG_SCROLL_UP;
	qwerty_layout.alternate[KEY_VOLUMEUP] = CHAR_SCROLL_UP;	
	
	qwerty_layout.normal[KEY_CAMERA] = CHAR_NOTHING;
	qwerty_layout.shifted[KEY_CAMERA] = CHAR_NOTHING;
	qwerty_layout.alternate[KEY_CAMERA] = CHAR_NOTHING;
	
	qwerty_layout.normal[KEY_HP] = CHAR_NOTHING;
	qwerty_layout.shifted[KEY_HP] = CHAR_NOTHING;
	qwerty_layout.alternate[KEY_HP] = CHAR_NOTHING;
	
	//##END QWERTY INITIALIZATION	
	
	memcpy(&qwertz_layout, &qwerty_layout, sizeof(keyboardLayout));
	qwertz_layout.normal[KEY_Y] = 'z';
	qwertz_layout.shifted[KEY_Y] = 'Z';
	
	qwertz_layout.normal[KEY_Z] = 'y';
	qwertz_layout.shifted[KEY_Z] = 'Y';

	memcpy(&azerty_layout, &qwerty_layout, sizeof(keyboardLayout));
	azerty_layout.normal[KEY_A] = 'q';
	azerty_layout.shifted[KEY_A] = 'Q';
	
	azerty_layout.normal[KEY_Q] = 'a';
	azerty_layout.shifted[KEY_Q] = 'A';
	
	azerty_layout.normal[KEY_W] = 'z';
	azerty_layout.shifted[KEY_W] = 'Z';
	
	azerty_layout.normal[KEY_Z] = 'w';
	azerty_layout.shifted[KEY_Z] = 'W';
	
	struct stat stFileInfo; 
  	int intStat; 

  	// Attempt to get the file attributes 
  	intStat = stat("/etc/keyboard", &stFileInfo); 
  	if(intStat == 0)
  	{
		int kbdfd = open("/etc/keyboard", O_RDONLY);
		char kbd_type[7];
		int rdbytes = read(kbdfd, kbd_type, 6);
		kbd_type[rdbytes] = '\0';
		
		if (!strcmp(kbd_type, "AZERTY"))
			current_layout = &azerty_layout;
		else if (!strcmp(kbd_type, "QWERTZ"))
			current_layout = &qwertz_layout;
		else
			current_layout = &qwerty_layout;
	}
	else
		current_layout = &qwerty_layout;
}

void show_console_menu() {
    char tmp[PATH_MAX];
    char* headers[] = {  "Console Menu",
                                "",
                                "",
                                NULL
    };
    static char* list[] = { "Enable Console",
                             NULL
    };
    
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
            {
		ui_print("Opening console...\n");                
		int console_error = run_console(NULL);

		if (console_error)
		    if (console_error == CONSOLE_FORCE_QUIT)
		        ui_print("Console was forcibly closed.\n");
		    else if (console_error == CONSOLE_FAILED_START)
		        ui_print("Console failed to start.\n");
		    else
		    {
		        ui_print("Closing console...\n"); //don't bother printing error to UI
		        fprintf(stderr, "Console closed with error %d.\n", console_error);
		    }		
		else
		    ui_print("Closing console...\n");
                break;
            }
        }
    }
}

static void
init_console()
{
	//clear keys
	ui_clear_key_queue();
	
	ui_set_background(BACKGROUND_ICON_NONE);
	init_keypad_layout();
	ui_console_begin();
}

static void
exit_console()
{
	//clear keys
	ui_clear_key_queue();
	
	ui_set_background(BACKGROUND_ICON_CLOCKWORK);
	ui_console_end();
}

static int 
create_subprocess(const char* cmd, const char* arg0, const char* arg1, pid_t* pProcessId) 
{
  char* devname;
  int ptm;
  pid_t pid;

  ptm = open("/dev/ptmx", O_RDWR); // | O_NOCTTY);
  if(ptm < 0)
  {
    fprintf(stderr, "[ cannot open /dev/ptmx - %s ]\n", strerror(errno));
    return -1;
  }
  
  fcntl(ptm, F_SETFD, FD_CLOEXEC);

  if(grantpt(ptm) || unlockpt(ptm) ||
     ((devname = (char*) ptsname(ptm)) == 0))
  {
    fprintf(stderr, "[ trouble with /dev/ptmx - %s ]\n", strerror(errno));
    return -1;
  }

  pid = fork();
  if(pid < 0) 
  {
    fprintf(stderr, "- fork failed: %s -\n", strerror(errno));
    return -1;
  }

  if(pid == 0)
  {
    int pts;

    setsid();

    pts = open(devname, O_RDWR);
    if(pts < 0) exit(-1);

    dup2(pts, 0);
    dup2(pts, 1);
    dup2(pts, 2);

    close(ptm);

    //so the profile detects if it's Safestrap console or not
    setenv("SAFESTRAP_CONSOLE", "1", 1);
    execl(cmd, cmd, arg0, arg1, NULL);
    exit(-1);
  } 
  else 
  {
    *pProcessId = (int) pid;
    return ptm;
  }
}

static void
set_nonblocking(int fd)
{
	//nonblocking mode
	int f = fcntl(fd, F_GETFL, 0);
	// Set bit for non-blocking flag
	f |= O_NONBLOCK;
	// Change flags on fd
	fcntl(fd, F_SETFL, f);
}

static void
send_escape_sequence(int ptmfd, const char* seq)
{
	char cmd[64];
	strcpy(cmd+1, seq);
	cmd[0] = ESC;
	write(ptmfd, &cmd, strlen(seq)+1); //+1 for the ESC, null terminator is not sent
}

static char
evaluate_key(int keycode, int shiftState, int altState)
{	
	if (altState)
		return current_layout->alternate[keycode];
	else if (shiftState)
		return current_layout->shifted[keycode];
	else
		return current_layout->normal[keycode];	
}

int run_console(const char* command)
{
	init_console();
				
	pid_t child;
	int childfd = create_subprocess("/sbin/bash", "-i", NULL, &child);
	
	if (childfd < 0)
	{
	    exit_console();
	    return CONSOLE_FAILED_START;
	}
	
	//status for the waitpid	
	int sts;
	int shell_error = 0;
	
	//buffer for pipe
	char buffer[5760+1];
	
	//set pipe to nonblocking
	set_nonblocking(childfd);
	
	//clear keys
	ui_clear_key_queue();
	
	//set the size
	struct winsize sz;
        sz.ws_row = ui_console_get_num_rows();
        sz.ws_col = ui_console_get_num_columns();
        sz.ws_xpixel = ui_console_get_width();
        sz.ws_ypixel = ui_console_get_height();
        ioctl(childfd, TIOCSWINSZ, &sz);
	
	ui_console_set_system_front_color(CONSOLE_DEFAULT_FRONT_COLOR);
	int force_quit = 0;
	
	//handle the i/o between the recovery and bash
	//manage the items to print here, but the actual printing is done in another thread
	while (1)
	{
		if (force_quit)
		{
			kill(child, SIGKILL);
			fprintf(stderr, "run_console: forcibly terminated.\n");
			waitpid(child, &sts, 0);
			break;
		}
	
		int rv = read(childfd, buffer, 5760);		
		
		if (rv <= 0)
		{
			if(errno == EAGAIN)
			{
				if (waitpid(child, &sts, WNOHANG))
					break;
			}
			else		
			{
				//not necessarilly an error (bash could have quit)
				fprintf(stderr, "run_console: there was a read error %d.\n", errno);
				waitpid(child, &sts, 0);
				break;
			}
		}
		else
		{	
			//if the string is read only partially, it won't be null terminated		  		
			buffer[rv] = 0;
			//fprintf(stderr, "run_console: received input of %d characters.\n", rv);
			ui_console_print(buffer);
			//fprintf(stderr, "run_console: input succesfully displayed.\n");
		}
		
		//evaluate one keyevent
		int keycode = ui_get_key();
		if (keycode != -1)
		{
			//"OK" + alt + BACKSPACE --> forcibly terminate bash
			
			if (ui_key_pressed(KEY_CENTER))
			{
				//ignore menu, shift and alt
				if (keycode == KEY_CENTER || keycode == KEY_LEFTALT || keycode == KEY_RIGHTALT ||
					keycode == KEY_LEFTSHIFT || keycode == KEY_RIGHTSHIFT)
					continue;
					
				char key = evaluate_key(keycode, 0, 0);
				int alt = ui_key_pressed(KEY_LEFTALT) | ui_key_pressed(KEY_RIGHTALT);
				int shift = ui_key_pressed(KEY_LEFTSHIFT) | ui_key_pressed(KEY_RIGHTSHIFT);
				int escape_key[2];
			
				if (alt && (key=='\b'))
				{
					force_quit = 1;
					continue;
				}
				
				int ascii_key[2];				
				
				if (islower(key)){					
					ascii_key[0] = toascii(toupper(key) - 'A' + 1);
				} else ascii_key[0] = toascii(key - 'A' + 1);
				write(childfd, &ascii_key[0], 1);		
				continue;
			}
			
			char key = evaluate_key(keycode, ui_key_pressed(KEY_LEFTSHIFT) | ui_key_pressed(KEY_RIGHTSHIFT), ui_key_pressed(KEY_LEFTALT) | ui_key_pressed(KEY_RIGHTALT));
			
			switch (key)
			{
				case 0:
					key = 0;
					break;
					
				case CHAR_NOTHING:
					break;
					
				case CHAR_SCROLL_DOWN:
					ui_console_scroll_down(1);
					break;
					
				case CHAR_SCROLL_UP:
					ui_console_scroll_up(1);
					break;
					
				case CHAR_BIG_SCROLL_DOWN:
					ui_console_scroll_down(8);
					break;
					
				case CHAR_BIG_SCROLL_UP:
					ui_console_scroll_up(8);
					break;
				case CHAR_ESCAPE:
					{				
					int escape_key2[2];
					escape_key2[0] = toascii(ESC);
					write(childfd, &escape_key2[0], 1);
					break;
					}
				case CHAR_KEY_UP:
					send_escape_sequence(childfd, "[A");
					break;
				case CHAR_KEY_LEFT:
					send_escape_sequence(childfd, "[D");
					break;
				case CHAR_KEY_RIGHT:
					send_escape_sequence(childfd, "[C");
					break;
				case CHAR_KEY_DOWN:	
					send_escape_sequence(childfd, "[B");
					break;
				default:
					write(childfd, &key, 1);
					break;
			}
		}
	
	}
	//check exit status
	if (WIFEXITED(sts)) 
	{
		if (WEXITSTATUS(sts) != 0) 
		{
			fprintf(stderr, "run_console: bash exited with status %d.\n",
			WEXITSTATUS(sts));
			shell_error = WEXITSTATUS(sts);
		}
	} 
	else if (WIFSIGNALED(sts)) 
	{
		fprintf(stderr, "run_console: bash terminated by signal %d.\n",
		WTERMSIG(sts));
		
		if (force_quit)
			shell_error = CONSOLE_FORCE_QUIT;
		else
			shell_error = 1;
	}
	
	close(childfd);
	exit_console();
	return shell_error;
}
