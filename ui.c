/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <getopt.h>
#include <termios.h>


#include "common.h"
#include "roots.h"
#include "minui/minui.h"
#include "recovery_ui.h"

extern int __system(const char *command);

#ifdef BOARD_HAS_NO_SELECT_BUTTON
static int gShowBackButton = 1;
#else
static int gShowBackButton = 0;
#endif

#define MAX_COLS 96
#define MAX_ROWS 32

#define MENU_MAX_COLS 64
#define MENU_MAX_ROWS 250

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 18

#define LED_OFF   	0x00
#define LED_ON		0x01
#define LED_BLINK	0x02
#define LED_BLINK_ONCE	0x03

#define LED_FILE_RED		"/sys/class/leds/red/brightness"
#define LED_FILE_GREEN		"/sys/class/leds/green/brightness"
#define LED_FILE_BLUE		"/sys/class/leds/blue/brightness"

#define KEYBOARD_BACKLIGHT_FILE	"/sys/class/leds/kpd_backlight_en/brightness"

// Console UI
#define CONSOLE_CHAR_WIDTH 10
#define CONSOLE_CHAR_HEIGHT 18

//max rows per screen
#define CONSOLE_BUFFER_ROWS 26
#define CONSOLE_TOTAL_ROWS (1000 + CONSOLE_BUFFER_ROWS)

//max supported columns per screen
#define CONSOLE_MAX_COLUMNS 100

//characters
#define CONSOLE_BEEP 7
#define CONSOLE_ESC 27

UIParameters ui_parameters = {
    6,       // indeterminate progress bar frames
    20,      // fps
    7,       // installation icon frames (0 == static image)
    13, 190, // installation icon overlay offset
};

enum
{
	LEFT_SIDE,
	CENTER_TILE,
	RIGHT_SIDE,
	NUM_SIDES
};

//the structure for 24bit color
typedef struct
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
} color24;

//the structure for 32bit color
typedef struct
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
} color32;

static pthread_mutex_t gUpdateMutex = PTHREAD_MUTEX_INITIALIZER;
static gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
static gr_surface *gInstallationOverlay;
static gr_surface *gProgressBarIndeterminate;
static gr_surface gProgressBarEmpty;
static gr_surface gProgressBarFill;
static int ui_has_initialized = 0;
static int ui_log_stdout = 1;

static pthread_cond_t led_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t led_mutex = PTHREAD_MUTEX_INITIALIZER;
static color24 led_color = {.r = 0, .g = 0, .b = 1};
static volatile unsigned int led_sts;

static const struct { gr_surface* surface; const char *name; } BITMAPS[] = {
    { &gBackgroundIcon[BACKGROUND_ICON_INSTALLING], "icon_installing" },
    { &gBackgroundIcon[BACKGROUND_ICON_ERROR],      "icon_error" },
    { &gBackgroundIcon[BACKGROUND_ICON_CLOCKWORK],  "icon_clockwork" },
    { &gBackgroundIcon[BACKGROUND_ICON_FIRMWARE_INSTALLING], "icon_firmware_install" },
    { &gBackgroundIcon[BACKGROUND_ICON_FIRMWARE_ERROR], "icon_firmware_error" },
    { &gProgressBarEmpty,               "progress_empty" },
    { &gProgressBarFill,                "progress_fill" },
    { NULL,                             NULL },
};

static int gCurrentIcon = 0;
static int gInstallingFrame = 0;

static enum ProgressBarType {
    PROGRESSBAR_TYPE_NONE,
    PROGRESSBAR_TYPE_INDETERMINATE,
    PROGRESSBAR_TYPE_NORMAL,
} gProgressBarType = PROGRESSBAR_TYPE_NONE;

// Progress bar scope of current operation
static float gProgressScopeStart = 0, gProgressScopeSize = 0, gProgress = 0;
static double gProgressScopeTime, gProgressScopeDuration;

// Set to 1 when both graphics pages are the same (except for the progress bar)
static int gPagesIdentical = 0;

//colors
static color32 background_color = {.r = 0, .g = 0, .b = 0, .a = 160 };
static color32 title_color = {.r = 60, .g = 255, .b = 110, .a = 255};
static color32 menu_color = {.r = 60, .g = 255, .b = 110, .a = 255};
static color32 menu_sel_color = {.r = 255, .g = 255, .b = 255, .a = 255};
static color32 script_color = {.r = 255, .g = 255, .b = 0, .a = 255};

// Log text overlay, displayed when a magic key is pressed
static char text[MAX_ROWS][MAX_COLS];
static int text_cols = 0, text_rows = 0;
static int text_col = 0, text_row = 0, text_top = 0;
static int show_text = 0;
static int show_text_ever = 0;   // has show_text ever been 1?

static char menu[MENU_MAX_ROWS][MENU_MAX_COLS];
static int show_menu = 0;
static int menu_top = 0, menu_items = 0, menu_sel = 0;
static int menu_show_start = 0;             // this is line which menu display is starting at

//console variables
static int console_screen_rows = 0;
static int console_screen_columns = 0;

//console system colors
static color24 console_header_color = {.r = 255, .g = 255, .b = 0};
static color24 console_background_color =  {.r = 0, .g = 0, .b = 0};
static color24 console_front_color =  {.r = 229, .g = 229, .b = 229};

static color24 console_term_colors[] =
{
	{ .r=0,   .g=0,   .b=0   }, //CLR30
	{ .r=205, .g=0,   .b=0   }, //CLR31
	{ .r=0,   .g=205, .b=0   }, //CLR32
	{ .r=205, .g=205, .b=0   }, //CLR33
	{ .r=0,   .g=0,   .b=238 }, //CLR34
	{ .r=205, .g=0,   .b=205 }, //CLR35
	{ .r=0,   .g=205, .b=205 }, //CLR36
	{ .r=229, .g=229, .b=229 }, //CLR37

	{ .r=127, .g=127, .b=127 }, //CLR90
	{ .r=255, .g=0,   .b=0   }, //CLR91
	{ .r=0,   .g=255, .b=0   }, //CLR92
	{ .r=255, .g=255, .b=0   }, //CLR93
	{ .r=92,  .g=91,  .b=255 }, //CLR94
	{ .r=255, .g=0,   .b=255 }, //CLR95
	{ .r=0,   .g=255, .b=255 }, //CLR96
	{ .r=255, .g=255, .b=255 }, //CLR97
};

//Rows increased for printing history
static char console_text[CONSOLE_TOTAL_ROWS][CONSOLE_MAX_COLUMNS];
static color24 console_text_color[CONSOLE_TOTAL_ROWS][CONSOLE_MAX_COLUMNS];
static color24 console_current_color;

static int console_top_row = 0;
static int console_force_top_row_on_text = 0;
static int console_force_top_row_reserve = 0;
static int console_cur_row = 0;
static int console_cur_column = 0;

static volatile int show_console = 0;
static volatile int console_refresh = 1;
static int console_escaped_state = 0;
static char console_escaped_buffer[64];
static char* console_escaped_sequence;

//synced via gUpdateMutex
static volatile int console_cursor_sts = 1;
static volatile clock_t console_cursor_last_update_time = 0;

// Key event input queue
static pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;
static int key_queue[2], key_queue_len = 0;
static volatile char key_pressed[KEY_MAX + 1];

// Return the current time as a double (including fractions of a second).
static double now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Draw the given frame over the installation overlay animation.  The
// background is not cleared or draw with the base icon first; we
// assume that the frame already contains some other frame of the
// animation.  Does nothing if no overlay animation is defined.
// Should only be called with gUpdateMutex locked.
static void draw_install_overlay_locked(int frame) {
    if (gInstallationOverlay == NULL) return;
    gr_surface surface = gInstallationOverlay[frame];
    int iconWidth = gr_get_width(surface);
    int iconHeight = gr_get_height(surface);
    gr_blit(surface, 0, 0, iconWidth, iconHeight,
            ui_parameters.install_overlay_offset_x,
            ui_parameters.install_overlay_offset_y);
}

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with gUpdateMutex locked.
static void draw_background_locked(int icon)
{
    gPagesIdentical = 0;
    gr_color(background_color.r, background_color.g, background_color.b, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    if (icon) {
        gr_surface surface = gBackgroundIcon[icon];
        int iconWidth = gr_get_width(surface);
        int iconHeight = gr_get_height(surface);
        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - iconHeight) / 2;
        gr_blit(surface, 0, 0, iconWidth, iconHeight, iconX, iconY);
        if (icon == BACKGROUND_ICON_INSTALLING) {
            draw_install_overlay_locked(gInstallingFrame);
        }
    }
}

// Draw the progress bar (if any) on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_progress_locked()
{
    if (gCurrentIcon == BACKGROUND_ICON_INSTALLING) {
        draw_install_overlay_locked(gInstallingFrame);
    }

    if (gProgressBarType != PROGRESSBAR_TYPE_NONE) {
        int iconHeight = gr_get_height(gBackgroundIcon[BACKGROUND_ICON_INSTALLING]);
        int width = gr_get_width(gProgressBarEmpty);
        int height = gr_get_height(gProgressBarEmpty);

        int dx = (gr_fb_width() - width)/2;
        int dy = (3*gr_fb_height() + iconHeight - 2*height)/4;

        // Erase behind the progress bar (in case this was a progress-only update)
        gr_color(0, 0, 0, 255);
        gr_fill(dx, dy, width, height);

        if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL) {
            float progress = gProgressScopeStart + gProgress * gProgressScopeSize;
            int pos = (int) (progress * width);

            if (pos > 0) {
                gr_blit(gProgressBarFill, 0, 0, pos, height, dx, dy);
            }
            if (pos < width-1) {
                gr_blit(gProgressBarEmpty, pos, 0, width-pos, height, dx+pos, dy);
            }
        }

        if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE) {
            static int frame = 0;
            gr_blit(gProgressBarIndeterminate[frame], 0, 0, width, height, dx, dy);
            frame = (frame + 1) % ui_parameters.indeterminate_frames;
        }
    }
}

static void draw_text_line(int row, const char* t) {
  if (t[0] != '\0') {
    gr_text(0, (row+1)*CHAR_HEIGHT-1, t);
  }
}

static void
draw_console_cursor(int row, int column, char letter)
{
	if (!console_cursor_sts)
		return;

	gr_color(console_front_color.r, console_front_color.g, console_front_color.b, 255);
	gr_fill_l(column * CONSOLE_CHAR_WIDTH, row * CONSOLE_CHAR_HEIGHT, (column+1)*CONSOLE_CHAR_WIDTH, (row+1)*CONSOLE_CHAR_HEIGHT);

	if (letter != '\0')
	{
		gr_color(console_background_color.r, console_background_color.g, console_background_color.b, 255);

		char text[2];
		text[0] = letter;
		text[1] = '\0';

		gr_text_l(column * CONSOLE_CHAR_WIDTH, (row+1)*CONSOLE_CHAR_HEIGHT-1, text);
	}
}

static void
draw_console_line(int row, const char* t, const color24* c) {

  char letter[2];
  letter[1] = '\0';

  int i = 0;

  while(t[i] != '\0')
  {
  	letter[0] = t[i];
  	gr_color(c[i].r, c[i].g, c[i].b, 255);
        gr_text_l(i * CONSOLE_CHAR_WIDTH, (row+1)*CONSOLE_CHAR_HEIGHT-1, letter);
	i++;
  }
}

static void
draw_console_locked()
{
  gr_color(console_background_color.r, console_background_color.g, console_background_color.b, 255);
  gr_fill(0, 0, gr_fb_width(), gr_fb_height());

	int draw_cursor = 0;

	int i;
	for (i = console_top_row; i < console_top_row + console_screen_rows; i++)
	{
		draw_console_line(i - console_top_row, console_text[i], console_text_color[i]);

		if (i == console_cur_row)
		{
			draw_cursor = 1;
			//break;
			continue;
		}
	}

	if(draw_cursor)
		draw_console_cursor(console_cur_row-console_top_row, console_cur_column, console_text[console_cur_row][console_cur_column]);
}

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_screen_locked(void)
{
    if (!ui_has_initialized) return;
	if (show_console)
  	  {
	  if (console_refresh)
   		draw_console_locked();
  		return;
  	  }

    draw_background_locked(gCurrentIcon);
    draw_progress_locked();

    if (show_text) {
        gr_color(background_color.r, background_color.g, background_color.b, background_color.a);
        gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int i = 0;
        int j = 0;
        int row = 0;            // current row that we are drawing on
        if (show_menu) {
            gr_color(menu_color.r, menu_color.g, menu_color.b, menu_color.a);
            gr_fill(0, (menu_top + menu_sel - menu_show_start) * CHAR_HEIGHT,
                    gr_fb_width(), (menu_top + menu_sel - menu_show_start + 1)*CHAR_HEIGHT+1);

            gr_color(title_color.r, title_color.g, title_color.b, title_color.a);
            for (i = 0; i < menu_top; ++i) {
                draw_text_line(i, menu[i]);
                row++;
            }

            if (menu_items - menu_show_start + menu_top >= MAX_ROWS)
                j = MAX_ROWS - menu_top;
            else
                j = menu_items - menu_show_start;

            gr_color(menu_color.r, menu_color.g, menu_color.b, menu_color.a);
            for (i = menu_show_start + menu_top; i < (menu_show_start + menu_top + j); ++i) {
                if (i == menu_top + menu_sel) {
                    gr_color(menu_sel_color.r, menu_sel_color.g, menu_sel_color.b, menu_sel_color.a);
                    draw_text_line(i - menu_show_start , menu[i]);
                    gr_color(menu_color.r, menu_color.g, menu_color.b, menu_color.a);
                } else {
                    gr_color(menu_color.r, menu_color.g, menu_color.b, menu_color.a);
                    draw_text_line(i - menu_show_start, menu[i]);
                }
                row++;
            }
            gr_fill(0, row*CHAR_HEIGHT+CHAR_HEIGHT/2-1,
                    gr_fb_width(), row*CHAR_HEIGHT+CHAR_HEIGHT/2+1);
        }

        gr_color(menu_color.r, menu_color.g, menu_color.b, menu_color.a);
        for (; row < text_rows; ++row) {
            draw_text_line(row, text[(row+text_top) % text_rows]);
        }
    }
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with gUpdateMutex locked.
static void update_screen_locked(void)
{
    if (!ui_has_initialized) return;
    draw_screen_locked();
    gr_flip();
}

// Updates only the progress bar, if possible, otherwise redraws the screen.
// Should only be called with gUpdateMutex locked.
static void update_progress_locked(void)
{
    if (!ui_has_initialized) return;
    if (show_text || !gPagesIdentical) {
        draw_screen_locked();    // Must redraw the whole screen
        gPagesIdentical = 1;
    } else {
        draw_progress_locked();  // Draw only the progress bar and overlays
    }
    gr_flip();
}

void ui_led_toggle(int state)
{
	pthread_mutex_lock(&led_mutex);

  if(state)
   led_sts = LED_ON;
  else
   led_sts = LED_OFF;
  
  pthread_cond_signal(&led_cond); 
  pthread_mutex_unlock(&led_mutex);
}

void ui_led_blink(int continuously)
{
	pthread_mutex_lock(&led_mutex);

  if(continuously)
   led_sts = LED_BLINK;
  else
   led_sts = LED_BLINK_ONCE;
  
  pthread_cond_signal(&led_cond); 
  pthread_mutex_unlock(&led_mutex);
  
}

static void
led_on(FILE* ledfp_r, FILE* ledfp_g, FILE* ledfp_b)
{
	if (led_color.r)
	{
		fwrite("1", 1, 1, ledfp_r);
		fflush(ledfp_r);
	}
	
	if (led_color.g)
	{
		fwrite("1", 1, 1, ledfp_g);
		fflush(ledfp_g);
	}
		
	if (led_color.b)
	{
		fwrite("1", 1, 1, ledfp_b);
		fflush(ledfp_b);
	}
}

static void
led_off(FILE* ledfp_r, FILE* ledfp_g, FILE* ledfp_b)
{
	fwrite("0", 1, 1, ledfp_r);
	fwrite("0", 1, 1, ledfp_g);
	fwrite("0", 1, 1, ledfp_b);
	
	fflush(ledfp_r);
	fflush(ledfp_g);
	fflush(ledfp_b);
}

static void*
led_thread(void *cookie)
{
  unsigned int state = 0;
  unsigned int waitperiod = 0;
  FILE *ledfp_r, *ledfp_g, *ledfp_b;
  
  ledfp_r = fopen(LED_FILE_RED, "w");
  ledfp_g = fopen(LED_FILE_GREEN, "w");
  ledfp_b = fopen(LED_FILE_BLUE, "w");

  while(1) 
  {
  	pthread_mutex_lock(&led_mutex);
  	
  	switch (led_sts)
  	{
  		case LED_OFF:
  			state = 0;
				led_off(ledfp_r, ledfp_g, ledfp_b);
  			
  			while (led_sts == LED_OFF) 
					pthread_cond_wait(&led_cond, &led_mutex);
					
				break;
				
			case LED_ON:
				state = 1;
				led_on(ledfp_r, ledfp_g, ledfp_b);	
  			
  			while (led_sts == LED_ON) 
					pthread_cond_wait(&led_cond, &led_mutex);
					
				break;
				
			case LED_BLINK_ONCE:
				state = 1;
			
				led_on(ledfp_r, ledfp_g, ledfp_b);	
				waitperiod = 500000;
				led_sts = LED_OFF;
				
				break;
				
			case LED_BLINK:
				state = state ? 0 : 1;
				
				if (state)
					led_on(ledfp_r, ledfp_g, ledfp_b);
				else
					led_off(ledfp_r, ledfp_g, ledfp_b);
			
				waitperiod = 500000;
				break;	
  	}
  	
  	pthread_mutex_unlock(&led_mutex);
  	
  	//when blinking, we want to finish it, not interrupt it
		if (waitperiod > 0)
		{
  		usleep(waitperiod);
  		waitperiod = 0;
  	}
  }
  
  fclose(ledfp_r);
  fclose(ledfp_g);
  fclose(ledfp_b);
  return NULL;
}

// Keeps the progress bar updated, even when the process is otherwise busy.
static void *progress_thread(void *cookie)
{
    double interval = 1.0 / ui_parameters.update_fps;
    for (;;) {
        double start = now();
        pthread_mutex_lock(&gUpdateMutex);

        int redraw = 0;

        // update the installation animation, if active
        // skip this if we have a text overlay (too expensive to update)
        if (gCurrentIcon == BACKGROUND_ICON_INSTALLING &&
            ui_parameters.installing_frames > 0) {
            gInstallingFrame =
                (gInstallingFrame + 1) % ui_parameters.installing_frames;
            redraw = 1;
        }

        // update the progress bar animation, if active
        // skip this if we have a text overlay (too expensive to update)
        if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE && !show_text) {
            redraw = 1;
        }

        // move the progress bar forward on timed intervals, if configured
        int duration = gProgressScopeDuration;
        if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && duration > 0) {
            double elapsed = now() - gProgressScopeTime;
            float progress = 1.0 * elapsed / duration;
            if (progress > 1.0) progress = 1.0;
            if (progress > gProgress) {
                gProgress = progress;
                redraw = 1;
            }
        }

        if (redraw) update_progress_locked();

        pthread_mutex_unlock(&gUpdateMutex);
        double end = now();
        // minimum of 20ms delay between frames
        double delay = interval - (end-start);
        if (delay < 0.02) delay = 0.02;
        usleep((long)(delay * 1000000));
    }
    return NULL;
}

// Reads input events, handles special hot keys, and adds to the key queue.
static void *
input_thread (void *cookie) 
{
  int rel_sum = 0;
  int fake_key = 0;
  int last_code = 0;
  unsigned keyheld;

  for (;;)
	  {
	    
	      // wait for the next key event
	    struct input_event ev;

	    
	    do
		    {
		    int repeat = ev_get(&ev, 0, keyheld);
		    
		    if (repeat != 1) {
				
			if (ev.type == EV_SYN){break;}
			
			// Check for an up/down key press
			if (ev.type == EV_KEY && ev.value == 1 ) {
				keyheld = 1;
				last_code = ev.code;
			} else { 
				keyheld = 0;
			}
		    } else if ( (ev.code == KEY_VOLUMEUP) || (ev.code == KEY_VOLUMEDOWN) || (ev.code == KEY_CENTER) ){
			ev.type = EV_KEY;
			ev.code = KEY_HP;
			ev.value = 1;
			continue;
		    } else { 
			// A return value of 1 means the last key should be repeated
			ev.type = EV_KEY;
			ev.code = last_code;
			ev.value = 1;
		    } 

		    if (ev.type == EV_REL)
		    {
			if (ev.code == REL_Y)
			{
					  
					  // accumulate the up or down motion reported by
					  // the trackball.  When it exceeds a threshold
					  // (positive or negative), fake an up/down
					  // key event.
					  rel_sum += ev.value;
					  if (rel_sum > 3)
						  {
						    fake_key = 1;
						    ev.type = EV_KEY;
						    ev.code = KEY_VOLUMEDOWN;
						    ev.value = 1;
						    rel_sum = 0;
						  }
					  else if (rel_sum < -3)
						  {
						    fake_key = 1;
						    ev.type = EV_KEY;
						    ev.code = KEY_VOLUMEUP;
						    ev.value = 1;
						    rel_sum = 0;
						  }
					}
			      } else if (ev.type == EV_ABS && (ev.code == KEY_VOLUMEUP || ev.code == KEY_VOLUMEDOWN)) {
			        fake_key = 1;
				ev.type = EV_KEY;
			      } else {
				rel_sum = 0;
			      }
		    }
	    while (ev.type != EV_KEY || ev.code > KEY_MAX);
	     pthread_mutex_lock (&key_queue_mutex);
	    if (!fake_key)
		    {
		      
			// our "fake" keys only report a key-down event (no
			// key-up), so don't record them in the key_pressed
			// table.
			key_pressed[ev.code] = ev.value;
		    }
	    fake_key = 0;
	    const int queue_max = sizeof (key_queue) / sizeof (key_queue[0]);

	    if (ev.value > 0 && key_queue_len < queue_max)
		    {
		      key_queue[key_queue_len++] = ev.code;
		      pthread_cond_signal (&key_queue_cond);
		    }
	    pthread_mutex_unlock (&key_queue_mutex);
	  }
  return NULL;
}


static void*
console_cursor_thread(void *cookie)
{
	while (show_console)
	{
		clock_t time_now = clock();
		double since_last_update = ((double)(time_now - console_cursor_last_update_time)) / CLOCKS_PER_SEC;

		if (since_last_update >= 0.5)
		{
			pthread_mutex_lock(&gUpdateMutex);
			console_cursor_sts = console_cursor_sts ? 0 : 1;
			console_cursor_last_update_time = time_now;
			update_screen_locked();
			pthread_mutex_unlock(&gUpdateMutex);
		}
		usleep(20000);
	}

	return NULL;
}

void ui_init(void)
{
    ui_has_initialized = 1;
    fprintf(stderr,"ui has initialized.\n");
    gr_init();
    ev_init();

    text_col = text_row = 0;
    text_rows = gr_fb_height() / CHAR_HEIGHT;
    if (text_rows > MAX_ROWS) text_rows = MAX_ROWS;
    text_top = 1;

    text_cols = gr_fb_width() / CHAR_WIDTH;
    if (text_cols > MAX_COLS - 1) text_cols = MAX_COLS - 1;

    int i;
    for (i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
        }
    }

    gProgressBarIndeterminate = malloc(ui_parameters.indeterminate_frames *
                                       sizeof(gr_surface));
    for (i = 0; i < ui_parameters.indeterminate_frames; ++i) {
        char filename[40];
        // "indeterminate01.png", "indeterminate02.png", ...
        sprintf(filename, "indeterminate%02d", i+1);
        int result = res_create_surface(filename, gProgressBarIndeterminate+i);
        if (result < 0) {
            LOGE("Missing bitmap %s\n(Code %d)\n", filename, result);
        }
    }

    if (ui_parameters.installing_frames > 0) {
        gInstallationOverlay = malloc(ui_parameters.installing_frames *
                                      sizeof(gr_surface));
        for (i = 0; i < ui_parameters.installing_frames; ++i) {
            char filename[40];
            // "icon_installing_overlay01.png",
            // "icon_installing_overlay02.png", ...
            sprintf(filename, "icon_installing_overlay%02d", i+1);
            int result = res_create_surface(filename, gInstallationOverlay+i);
            if (result < 0) {
                LOGE("Missing bitmap %s\n(Code %d)\n", filename, result);
            }
        }

        // Adjust the offset to account for the positioning of the
        // base image on the screen.
        if (gBackgroundIcon[BACKGROUND_ICON_INSTALLING] != NULL) {
            gr_surface bg = gBackgroundIcon[BACKGROUND_ICON_INSTALLING];
            ui_parameters.install_overlay_offset_x +=
                (gr_fb_width() - gr_get_width(bg)) / 2;
            ui_parameters.install_overlay_offset_y +=
                (gr_fb_height() - gr_get_height(bg)) / 2;
        }
    } else {
        gInstallationOverlay = NULL;
    }
    pthread_t t;
    pthread_create(&t, NULL, progress_thread, NULL);
    pthread_create(&t, NULL, input_thread, NULL);
    pthread_create(&t, NULL, led_thread, NULL);

}

char *ui_copy_image(int icon, int *width, int *height, int *bpp) {
    pthread_mutex_lock(&gUpdateMutex);
    draw_background_locked(icon);
    *width = gr_fb_width();
    *height = gr_fb_height();
    *bpp = sizeof(gr_pixel) * 8;
    int size = *width * *height * sizeof(gr_pixel);
    char *ret = malloc(size);
    if (ret == NULL) {
        LOGE("Can't allocate %d bytes for image\n", size);
    } else {
        memcpy(ret, gr_fb_data(), size);
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return ret;
}

void ui_set_background(int icon)
{
    pthread_mutex_lock(&gUpdateMutex);
    gCurrentIcon = icon;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_indeterminate_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    if (gProgressBarType != PROGRESSBAR_TYPE_INDETERMINATE) {
        gProgressBarType = PROGRESSBAR_TYPE_INDETERMINATE;
        update_progress_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_progress(float portion, int seconds)
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NORMAL;
    gProgressScopeStart += gProgressScopeSize;
    gProgressScopeSize = portion;
    gProgressScopeTime = now();
    gProgressScopeDuration = seconds;
    gProgress = 0;
    update_progress_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_set_progress(float fraction)
{
    pthread_mutex_lock(&gUpdateMutex);
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && fraction > gProgress) {
        // Skip updates that aren't visibly different.
        int width = gr_get_width(gProgressBarIndeterminate[0]);
        float scale = width * gProgressScopeSize;
        if ((int) (gProgress * scale) != (int) (fraction * scale)) {
            gProgress = fraction;
            update_progress_locked();
        }
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_reset_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NONE;
    gProgressScopeStart = gProgressScopeSize = 0;
    gProgressScopeTime = gProgressScopeDuration = 0;
    gProgress = 0;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 2048, fmt, ap);
    va_end(ap);

    if (ui_log_stdout)
        fputs(buf, stdout);

    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_printlogtail(int nb_lines) {
    char * log_data;
    char tmp[PATH_MAX];
    FILE * f;
    int line=0;
    //don't log output to recovery.log
    ui_log_stdout=0;
    sprintf(tmp, "tail -n %d /tmp/recovery.log > /tmp/tail.log", nb_lines);
    __system(tmp);
    f = fopen("/tmp/tail.log", "rb");
    if (f != NULL) {
        while (line < nb_lines) {
            log_data = fgets(tmp, PATH_MAX, f);
            if (log_data == NULL) break;
            ui_print("%s", tmp);
            line++;
        }
        fclose(f);
    }
    ui_log_stdout=1;
}

void ui_reset_text_col()
{
    pthread_mutex_lock(&gUpdateMutex);
    text_col = 0;
    pthread_mutex_unlock(&gUpdateMutex);
}

#define MENU_ITEM_HEADER " - "
#define MENU_ITEM_HEADER_LENGTH strlen(MENU_ITEM_HEADER)

int ui_start_menu(char** headers, char** items, int initial_selection) {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        for (i = 0; i < text_rows; ++i) {
            if (headers[i] == NULL) break;
            strncpy(menu[i], headers[i], text_cols-1);
            menu[i][text_cols-1] = '\0';
        }
        menu_top = i;
        for (; i < MENU_MAX_ROWS; ++i) {
            if (items[i-menu_top] == NULL) break;
            strcpy(menu[i], MENU_ITEM_HEADER);
            strncpy(menu[i] + MENU_ITEM_HEADER_LENGTH, items[i-menu_top], text_cols-1 - MENU_ITEM_HEADER_LENGTH);
            menu[i][text_cols-1] = '\0';
        }

        if (gShowBackButton) {
            strcpy(menu[i], " - +++++Go Back+++++");
            ++i;
        }

        menu_items = i - menu_top;
        show_menu = 1;
        menu_sel = menu_show_start = initial_selection;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
    if (gShowBackButton) {
        return menu_items - 1;
    }
    return menu_items;
}

int ui_menu_select(int sel) {
    int old_sel;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0) {
        old_sel = menu_sel;
        menu_sel = sel;

        if (menu_sel < 0) menu_sel = menu_items + menu_sel;
        if (menu_sel >= menu_items) menu_sel = menu_sel - menu_items;


        if (menu_sel < menu_show_start && menu_show_start > 0) {
            menu_show_start = menu_sel;
        }

        if (menu_sel - menu_show_start + menu_top >= text_rows) {
            menu_show_start = menu_sel + menu_top - text_rows + 1;
        }

        sel = menu_sel;

        if (menu_sel != old_sel) update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return sel;
}

void ui_end_menu() {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0 && text_rows > 0 && text_cols > 0) {
        show_menu = 0;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_text_visible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&gUpdateMutex);
    return visible;
}

int ui_get_key()
{
	int key = -1;
	pthread_mutex_lock(&key_queue_mutex);

	if (key_queue_len != 0)
	{
		key = key_queue[0];
		memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
	}

	pthread_mutex_unlock(&key_queue_mutex);
	return key;
}

int ui_text_ever_visible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int ever_visible = show_text_ever;
    pthread_mutex_unlock(&gUpdateMutex);
    return ever_visible;
}

void ui_show_text(int visible)
{
    pthread_mutex_lock(&gUpdateMutex);
    show_text = visible;
    if (show_text) show_text_ever = 1;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

// Return true if USB is connected.
static int usb_connected() {
    int fd = open("/sys/devices/platform/cpcap_usb_connected", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr,"Failed to open /sys/devices/platform/cpcap_usb_connected\n");
        return 0;
    } else {
        fprintf(stderr,"USB detected, fd=%d\n",fd);
        return fd;
    }
}

int ui_wait_key()
{
    pthread_mutex_lock(&key_queue_mutex);
    while (key_queue_len == 0) {
        pthread_cond_wait(&key_queue_cond, &key_queue_mutex);
    }

    int key = key_queue[0];
    memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

int ui_key_pressed(int key)
{
    // This is a volatile static array, don't bother locking
    return key_pressed[key];
}

void ui_clear_key_queue() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}

void ui_set_show_text(int value) {
    show_text = value;
}

void ui_set_showing_back_button(int showBackButton) {
    gShowBackButton = showBackButton;
}

int ui_get_showing_back_button() {
    return gShowBackButton;
}

int ui_get_num_columns()
{
return text_cols;
}

void ui_console_begin()
{
	//enable keyboard backlight
    	FILE* keyboardfd = fopen(KEYBOARD_BACKLIGHT_FILE, "w");
    	fwrite("1", 1, 1, keyboardfd);
    	fclose(keyboardfd);
        
	//check to see if usb is connected, output results to log
	usb_connected();	
	pthread_mutex_lock(&gUpdateMutex);
	show_console = 1;
	console_refresh = 1;
	console_cursor_sts = 1;
	console_cursor_last_update_time = clock();
	console_top_row = 0;
	console_cur_row = 0;
	console_cur_column = 0;
	console_escaped_state = 0;

	//calculate the number of columns and rows
	console_screen_rows = ui_console_get_height() / CONSOLE_CHAR_HEIGHT;
	console_screen_columns = ui_console_get_width() / CONSOLE_CHAR_WIDTH + 1; //+1 for null terminator
	fprintf(stderr,"rows=%d, cols=%d\n",console_screen_rows,console_screen_columns);
	console_force_top_row_on_text = 0;
	console_force_top_row_reserve = 1 - console_screen_rows;

	memset(console_text, ' ', (CONSOLE_TOTAL_ROWS) * (console_screen_columns));
	memset(console_text_color, 0, (CONSOLE_TOTAL_ROWS) * (console_screen_columns) * sizeof(color24));

	int i;
	for (i = 0; i < (CONSOLE_TOTAL_ROWS); i++)
		console_text[i][console_screen_columns - 1] = '\0';

	console_current_color = console_front_color;

	pthread_t t;
	pthread_create(&t, NULL, console_cursor_thread, NULL);

	update_screen_locked();
	pthread_mutex_unlock(&gUpdateMutex);
}

void ui_console_end()
{
	//disable keyboard backlight
    	FILE* keyboardfd = fopen(KEYBOARD_BACKLIGHT_FILE, "w");
    	fwrite("0", 1, 1, keyboardfd);
    	fclose(keyboardfd);
	
	pthread_mutex_lock(&gUpdateMutex);
	show_console = 0;
	update_screen_locked();
	console_screen_rows = 0;
	console_screen_columns = 0;
	pthread_mutex_unlock(&gUpdateMutex);
}

void ui_console_begin_update()
{
	pthread_mutex_lock(&gUpdateMutex);
	console_refresh = 0;
	pthread_mutex_unlock(&gUpdateMutex);
}

void ui_console_end_update()
{
	pthread_mutex_lock(&gUpdateMutex);
	console_refresh = 1;
	update_screen_locked();
	pthread_mutex_unlock(&gUpdateMutex);
}

int ui_console_get_num_rows()
{
	return console_screen_rows;
}

int ui_console_get_num_columns()
{
	return console_screen_columns - 1;
}

//landscape, swap height and width
int ui_console_get_width()
{
	return gr_fb_height();
}

int ui_console_get_height()
{
	return gr_fb_width();
}

void ui_console_scroll_up(int num_rows)
{
	pthread_mutex_lock(&gUpdateMutex);

	if (console_top_row - num_rows < 0)
		console_top_row = 0;
	else
		console_top_row -= num_rows;

	update_screen_locked();
	pthread_mutex_unlock(&gUpdateMutex);
}

void ui_console_scroll_down(int num_rows)
{
	int max_row_top = console_cur_row - console_screen_rows + 1;

	if (max_row_top < console_force_top_row_on_text)
		max_row_top = console_force_top_row_on_text;

	if (max_row_top < 0)
		max_row_top = 0;

	pthread_mutex_lock(&gUpdateMutex);
	if (console_top_row + num_rows > max_row_top)
		console_top_row = max_row_top;
	else
		console_top_row += num_rows;

	update_screen_locked();
	pthread_mutex_unlock(&gUpdateMutex);
}

void ui_console_get_system_front_color(int which, unsigned char* r, unsigned char* g, unsigned char* b)
{
	color24 c = {.r = 0, .g = 0, .b = 0};

	switch(which)
	{
		case CONSOLE_HEADER_COLOR:
			c = console_header_color;
			break;

		case CONSOLE_DEFAULT_BACKGROUND_COLOR:
			c = console_background_color;
			break;

		case CONSOLE_DEFAULT_FRONT_COLOR:
			c = console_front_color;
			break;
	}

	*r = c.r;
	*g = c.g;
	*b = c.b;
}

void ui_console_set_system_front_color(int which)
{
	switch(which)
	{
		case CONSOLE_HEADER_COLOR:
			console_current_color = console_header_color;
			break;

		case CONSOLE_DEFAULT_BACKGROUND_COLOR:
			console_current_color = console_background_color;
			break;

		case CONSOLE_DEFAULT_FRONT_COLOR:
			console_current_color = console_front_color;
			break;
	}
}

void ui_console_get_front_color(unsigned char* r, unsigned char* g, unsigned char* b)
{
	*r = console_current_color.r;
	*g = console_current_color.g;
	*b = console_current_color.b;
}

void ui_console_set_front_color(unsigned char r, unsigned char g, unsigned char b)
{
	console_current_color.r = r;
	console_current_color.g = g;
	console_current_color.b = b;
}

void console_set_front_term_color(int ascii_code)
{
	if (ascii_code >= 30 && ascii_code < 37)
		ui_console_set_front_color(
			console_term_colors[ascii_code - 30].r,
			console_term_colors[ascii_code - 30].g,
			console_term_colors[ascii_code - 30].b);
	else if (ascii_code >= 90 && ascii_code < 97)
		ui_console_set_front_color(
			console_term_colors[ascii_code - 90 + 8].r,
			console_term_colors[ascii_code - 90 + 8].g,
			console_term_colors[ascii_code - 90 + 8].b);
}

static void
console_put_char(char c)
{
	switch(c)
	{
		case '\n':
			//fprintf(stderr, "Row %d, Column %d, Char \"LINE BREAK\"\n", console_cur_row, console_cur_column);
			console_cur_row++;
			console_force_top_row_reserve++;
			break;

		case '\r':
			//fprintf(stderr, "Row %d, Column %d, Char \"CARRIAGE RETURN\"\n", console_cur_row, console_cur_column);
			console_cur_column = 0;
			break;

		case '\t':
			//fprintf(stderr, "Row %d, Column %d, Char \"TAB\"\n", console_cur_row, console_cur_column);
			//tab is per 5
			{
			int end = console_cur_column + (5 - console_cur_column % 5);

			if (end >= console_screen_columns - 2)
			{
				int i;
				for (i = console_cur_column; i < console_screen_columns - 1; i++)
					console_text[console_cur_row][i] = ' ';

				console_cur_column = 0;
				console_cur_row++;
				console_force_top_row_reserve++;
			}
			else
			{
				int i;
				for (i = console_cur_column; i < end; i++)
					console_text[console_cur_row][i] = ' ';

				console_cur_column = end;
			}
			}
			break;

		case '\b':
			//fprintf(stderr, "Row %d, Column %d, Char \"BACKSPACE\"\n", console_cur_row, console_cur_column);
			if (console_cur_column == 0)
			{
				if (console_cur_row == 0)
					break;

				console_cur_column = console_screen_columns - 2;
				console_cur_row--;
			}
			else
				console_cur_column--;
			break;

		case CONSOLE_BEEP: //BELL - use LED for that
			ui_led_blink(0);
			vibrate(30);
			break;

		default:
			console_text[console_cur_row][console_cur_column] = c;
			console_text_color[console_cur_row][console_cur_column] = console_current_color;
			//fprintf(stderr, "Row %d, Column %d, Char %d\n", console_cur_row, console_cur_column, c);
			console_cur_column++;

			if (console_cur_column > console_screen_columns - 2)
			{
				console_cur_column = 0;
				console_cur_row++;
				console_force_top_row_reserve++;
			}
			break;
	}

	if (console_cur_row == CONSOLE_TOTAL_ROWS)
	{
		int shift = console_cur_row - (CONSOLE_TOTAL_ROWS - CONSOLE_BUFFER_ROWS);
		//fprintf(stderr, "Shifting the rows by %d.\n", shift);
		int j;

		for (j = 0; j < CONSOLE_TOTAL_ROWS - CONSOLE_BUFFER_ROWS; j++)
		{
			memcpy(console_text[j], console_text[j + shift], console_screen_columns);
			memcpy(console_text_color[j], console_text_color[j + shift], console_screen_columns * sizeof(color24));
		}

		for (j = CONSOLE_TOTAL_ROWS - CONSOLE_BUFFER_ROWS; j < CONSOLE_TOTAL_ROWS; j++)
		{
			memset(console_text[j], ' ', console_screen_columns);
			console_text[j][console_screen_columns - 1] = '\0';
			memset(console_text_color[j], 0, console_screen_columns * sizeof(color24));
		}

		console_cur_row -= shift;
		console_force_top_row_on_text -= shift;
	}
}

static void
console_unescape()
{
	if (!show_console) return;	
	int len = strlen(console_escaped_buffer);
	int was_unescaped = 0;
	int noSqrBrackets = 0;
	int noRoundBracketsLeft = 0;
	int noRoundBracketsRight = 0;
	int noQuestionMarks = 0;
	int parameters[32];
	int noParameters = 0;

	char argument = '\0';
	char *ptr;

	memset(parameters, 0, sizeof(int) * 32);

	//first parse it
	for (ptr = console_escaped_buffer; *ptr != '\0'; ++ptr)
	{
		if (*ptr == '[')
			noSqrBrackets++;
		else if (*ptr == '(')
			noRoundBracketsLeft++;
		else if (*ptr == ')')
			noRoundBracketsRight++;
		else if (*ptr == '?')
			noQuestionMarks++;
		else if (*ptr == ';')
			noParameters++;
		else if (*ptr >= '0' && *ptr <= '9')
			parameters[noParameters] = parameters[noParameters] * 10 + (*ptr - '0');
		else
		{
			argument = *ptr;
			break;
		}
	}

	//was used for indexing, so increment it
	noParameters++;

	int i, j;
	for (i = 0; i < noParameters; i++)
		//fprintf(stderr, " %d", parameters[i]);

	//fprintf(stderr, "\n");

	if (noSqrBrackets == 1 && noRoundBracketsLeft == 0 && noRoundBracketsRight == 0 && noQuestionMarks == 0)
	{
		switch (argument)
		{
			//======================================================================
			// UPPERCASE
			//======================================================================

			//move up n lines, but not out of screen
			case 'A':
				console_cur_row -= parameters[0];
				if (console_force_top_row_on_text > console_cur_row)
					console_cur_row = console_force_top_row_on_text;

				//set the top reserve
				console_force_top_row_reserve = 1 - (console_force_top_row_on_text +
					console_screen_rows - console_cur_row);
				was_unescaped = 1;
				break;

			//move down n lines, but not out of screen
			case 'B':
				console_cur_row += parameters[0];
				if (console_cur_row >= console_force_top_row_on_text + console_screen_rows)
					console_cur_row = console_force_top_row_on_text + console_screen_rows - 1;

				//set the top reserve
				console_force_top_row_reserve = 1 - (console_force_top_row_on_text +
					console_screen_rows - console_cur_row);
				was_unescaped = 1;
				break;

			//move right, but not out of the line
			case 'C':
				console_cur_column += parameters[0];
				if ( console_cur_column >= (console_screen_columns - 1) )
					console_cur_column = console_screen_columns - 2;

				was_unescaped = 1;
				break;

			//move left, but not out of the line
			case 'D':
				console_cur_column -= parameters[0];

				if (console_cur_column < 0)
					console_cur_column = 0;

				was_unescaped = 1;
				break;

			//cursor to top-left + offset
			case 'H':
				if (parameters[0] >= console_screen_rows)
					parameters[0] = console_screen_rows - 1;

				if (parameters[1] >= (console_screen_columns - 1) )
					parameters[1] = console_screen_columns - 2;

				console_cur_row = console_top_row + parameters[0];
				console_cur_column = parameters[1];
				console_force_top_row_on_text = console_top_row;
				console_force_top_row_reserve = 1 - (console_top_row +
					console_screen_rows - console_cur_row);

				was_unescaped = 1;
				break;

			
			//clear below cursor
			case 'J':
				for (j = console_cur_column; j < (console_screen_columns - 1); j++)
				{
					console_text[console_cur_row][j] = ' ';
					console_text_color[console_cur_row][j].r = 0;
					console_text_color[console_cur_row][j].g = 0;
					console_text_color[console_cur_row][j].b = 0;
				}

				for (j = console_cur_row + 1; j < CONSOLE_TOTAL_ROWS; j++)
				{
					memset(console_text[j], ' ', console_screen_columns);
					console_text[j][console_screen_columns - 1] = '\0';
					memset(console_text_color[j], 0, console_screen_columns * sizeof(color24));
				}
				was_unescaped = 1;
				break;
			
			//clear from cursor cursor
			case 'K':
				if (parameters[0] == 0)
				{
					for (j = console_cur_column; j < (console_screen_columns - 1); j++)
					{
						console_text[console_cur_row][j] = ' ';
						console_text_color[console_cur_row][j].r = 0;
						console_text_color[console_cur_row][j].g = 0;
						console_text_color[console_cur_row][j].b = 0;
					}
				}
				else if (parameters[0] == 1)
				{
					for (j = 0; j <= console_cur_column; j++)
					{
						console_text[console_cur_row][j] = ' ';
						console_text_color[console_cur_row][j].r = 0;
						console_text_color[console_cur_row][j].g = 0;
						console_text_color[console_cur_row][j].b = 0;
					}
				}
				else if (parameters[0] == 2)
				{
					for (j = 0; j < (console_screen_columns - 1); j++)
					{
						console_text[console_cur_row][j] = ' ';
						console_text_color[console_cur_row][j].r = 0;
						console_text_color[console_cur_row][j].g = 0;
						console_text_color[console_cur_row][j].b = 0;
					}
				}

				was_unescaped = 1;
				break;
			//======================================================================
			// LOWERCASE
			//======================================================================

			//only coloring, ignore bolding, italic etc.
			//background color changing is not supported
			case 'm':

				//consider 'm' to be succesfully unescaped in all cases
				was_unescaped = 1;

				for (i = 0; i < noParameters; i++)
				{
					switch (parameters[i])
					{
						case 0: //reset
						case 39: //default text color
							ui_console_set_system_front_color(CONSOLE_DEFAULT_FRONT_COLOR);
							break;
						case 30:
						case 31:
						case 32:
						case 33:
						case 34:
						case 35:
						case 36:
						case 37:
						case 90:
						case 91:
						case 92:
						case 93:
						case 94:
						case 95:
						case 96:
						case 97:
							console_set_front_term_color(parameters[i]);
							break;
					}
				}
		}
	}

	if (!was_unescaped) //send it ala text then
	{
		console_put_char('^');
		int e;
		for (e = 0; console_escaped_buffer[e] != '\0'; e++)
			console_put_char(console_escaped_buffer[e]);
	}
}

static void
console_put_escape_sequence(char c)
{
	*console_escaped_sequence = c;
	console_escaped_sequence++;

	if (c != '[' && c != '(' && c != '?' && c != ')' && c != ';' && !(c >= '0' && c <= '9'))
	{
		*console_escaped_sequence = '\0';
		//fprintf(stderr, "Escape character: %s\n", console_escaped_buffer);
		console_unescape();
		console_escaped_state = 0;
	}
}

void ui_console_print(const char *text)
{
	pthread_mutex_lock(&gUpdateMutex);
	const char *ptr;
	for (ptr = text; *ptr != '\0'; ++ptr)
	{
		//parse escape characters here
		if (*ptr == CONSOLE_ESC)
		{
			console_escaped_state = 1;
			console_escaped_buffer[0] = '\0';
			console_escaped_sequence = &(console_escaped_buffer[0]);
			continue;
		}
		else if (!console_escaped_state)
			console_put_char(*ptr);
		else
			console_put_escape_sequence(*ptr);
	}

	if (console_force_top_row_reserve > 0)
	{
		console_force_top_row_on_text += console_force_top_row_reserve;
		console_force_top_row_reserve = 0;
	}

	console_top_row = console_force_top_row_on_text;
	if (console_top_row < 0)
		console_top_row = 0;

	console_cursor_sts = 1;
	console_cursor_last_update_time = clock();
	update_screen_locked();
	pthread_mutex_unlock(&gUpdateMutex);
}
