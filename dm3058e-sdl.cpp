/*
 * RIGOL DM3058E(E)
 *
 * December 29, 2020
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 *
 */

#include <SDL.h>
#include <SDL_ttf.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#define FL __FILE__,__LINE__

/*
 * Should be defined in the Makefile to pass to the compiler from
 * the github build revision
 *
 */
#ifndef BUILD_VER 
#define BUILD_VER 000
#endif

#ifndef BUILD_DATE
#define BUILD_DATE " "
#endif

#define SSIZE 1024

#define ee ""
#define uu "\u00B5"
#define kk "k"
#define MM "M"
#define mm "m"
#define nn "n"
#define pp "p"
#define dd "\u00B0"
#define oo "\u03A9"
#define SKIP "NONE"

#define CMODE_USB 1
#define CMODE_SERIAL 2
#define CMODE_NONE 0

struct mmode_s {
	char scpi[50];
	char label[50];
	char query[50];
	char range[50];
	char units[10];
};

#define MMODES_VOLT_DC 0
#define MMODES_VOLT_AC 1
#define MMODES_CURR_DC 2
#define MMODES_CURR_AC 3
#define MMODES_RES 4
#define MMODES_CAP 5
#define MMODES_CONT 6
#define MMODES_FRES 7
#define MMODES_DIOD 8
#define MMODES_FREQ 9
#define MMODES_PER 10



#define MMODES_MAX 10


#define READSTATE_NONE		0
#define READSTATE_READING_MEASURE 1
#define READSTATE_FINISHED_MEASURE 2
#define READSTATE_READING_FUNCTION 3
#define READSTATE_FINISHED_FUNCTION 4
#define READSTATE_READING_VAL 5
#define READSTATE_FINISHED_VAL 6
#define READSTATE_READING_RANGE 7
#define READSTATE_FINISHED_RANGE 8
#define READSTATE_READING_CONTLIMIT 9
#define READSTATE_FINISHED_CONTLIMIT 10
#define READSTATE_FINISHED_ALL 11
#define READSTATE_DONE		12
#define READSTATE_ERROR 999

#define READ_BUF_SIZE 4096

struct mmode_s mmodes[] = { 
	{"DCV", "Volts DC", ":MEAS:VOLT:DC?\r\n", ":MEAS:VOLT:DC:RANG?\r\n", "V DC"}, 
	{"ACV", "Volts AC", ":MEAS:VOLT:AC?\r\n", ":MEAS:VOLT:AC:RANG?\r\n", "V AC"},
	{"DCI", "Current DC", ":MEAS:CURR:DC?\r\n", ":MEAS:CURR:DC:RANG?\r\n", "A DC"},
	{"ACI", "Current AC", ":MEAS:CURR:AC?\r\n", ":MEAS:CURR:AC:RANG?\r\n", "A AC"},
	{"2WR", "Resistance", ":MEAS:RES?\r\n",":MEAS:RES:RANG?\r\n", oo },
	{"CAP", "Capacitance", ":MEAS:CAP?\r\n", ":MEAS:CAP:RANG?\r\n", "F"},
	{"CONT", "Continuity", ":MEAS:CONT?\r\n",SKIP, oo},
	{"4WR", "4WResistance", ":MEAS:FRES?\r\n", ":MEAS:FRES:RANG?\r\n", oo },
	{"DIODE", "Diode", ":MEAS:DIOD?\r\n",SKIP, "V"},
	{"FREQ", "Frequency", ":MEAS:FREQ?\r\n",":MEAS:FREQ:RANG?\r\n", "Hz" },
	{"PERIOD", "Period", ":MEAS:PER?\r\n",":MEAS:PER:RANG?\r\n", "s"}
};

const char SCPI_FUNC[] = ":FUNC?\r\n";
const char SCPI_MEAS[] = ":MEAS?\r\n";

//const char SCPI_VAL2[] = "VAL2?\r\n";//RIGOL DOESNT SUPPORT THAT
//const char SCPI_CONT_THRESHOLD[] = "SENS:CONT:THR?\r\n";//RIGOL DOESNT SUPPORT THAT
//const char SCPI_LOCAL[] = "SYST:LOC\r\n";//RIGOL DOESNT SUPPORT THAT

const char SEPARATOR_DP[] = ".";

struct serial_params_s {
	char *device;
	int fd, n;
	int cnt, size, s_cnt;
	struct termios oldtp, newtp;
};


struct glb {
	uint8_t debug;
	uint8_t quiet;
	uint16_t flags;
	uint16_t error_flag;
	char *output_file;
	char *device;

	int usb_fhandle;

	int comms_mode;
	char *com_address;
	char *serial_parameters_string; // this is the raw from the command line
	struct serial_params_s serial_params; // this is the decoded version

	int mode_index;
	int read_state;
	char read_buffer[READ_BUF_SIZE];
	char *bp;
	ssize_t bytes_remaining;

	int cont_threshold;
	double v;
	char value[READ_BUF_SIZE];
	char func[READ_BUF_SIZE];
	char range[READ_BUF_SIZE];

	int interval;
	int font_size;
	int window_width, window_height;
	int wx_forced, wy_forced;
	SDL_Color font_color_pri, font_color_sec, background_color;
};

/*
 * A whole bunch of globals, because I need
 * them accessible in the Windows handler
 *
 * So many of these I'd like to try get away from being
 * a global.
 *
 */
struct glb *glbs;

/*
 * Test to see if a file exists
 *
 * Does not test anything else such as read/write status
 *
 */
bool fileExists(const char *filename) {
	struct stat buf;
	return (stat(filename, &buf) == 0);
}


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220248
  Function Name	: init
  Returns Type	: int
  ----Parameter List
  1. struct glb *g ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int init(struct glb *g) {
	g->read_state = READSTATE_NONE;
	g->mode_index = MMODES_MAX;
	g->cont_threshold = 10.0; // ohms
	g->debug = 0;
	g->quiet = 0;
	g->flags = 0;
	g->error_flag = 0;
	g->output_file = NULL;
	g->interval = 100000; // 100ms / 100,000us interval of sleeping between frames
	g->device = NULL;
	g->comms_mode = CMODE_NONE;

	g->serial_parameters_string = NULL;

	g->font_size = 60;
	g->window_width = 400;
	g->window_height = 100;
	g->wx_forced = 0;
	g->wy_forced = 0;

	g->font_color_pri =  { 10, 200, 10 };
	g->font_color_sec =  { 200, 200, 10 };
	g->background_color = { 0, 0, 0 };

	return 0;
}

void show_help(void) {
	fprintf(stdout,"DM3058E Multimeter display\r\n"
			"By Paul L Daniels / pldaniels@gmail.com\r\n"
			"Build %d / %s\r\n"
			"\r\n"
			" [-p <usbtmc path, ie /dev/usbtmc2>] \r\n"
			"\r\n"
			"\t-h: This help\r\n"
			"\t-d: debug enabled\r\n"
			"\t-q: quiet output\r\n"
			"\t-v: show version\r\n"
			"\t-z <font size in pt>\r\n"
			"\t-cv <volts colour, a0a0ff>\r\n"
			"\t-ca <amps colour, ffffa0>\r\n"
			"\t-cb <background colour, 101010>\r\n"
			"\t-t <interval> (sleep delay between samples, default 100,000us)\r\n"
			"\t-p <comport>: Set the com port for the meter, eg: -p /dev/ttyUSB0\r\n"
			"\t-s <115200|57600|38400|19200|9600> serial speed (default 115200)\r\n"
			"\r\n"
			"\texample: DM3058E-sdl -p /dev/ttyUSB0 -s 38400\r\n"
			, BUILD_VER
			, BUILD_DATE 
			);
} 


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220258
  Function Name	: parse_parameters
  Returns Type	: int
  ----Parameter List
  1. struct glb *g,
  2.  int argc,
  3.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int parse_parameters(struct glb *g, int argc, char **argv ) {
	int i;

	if (argc == 1) {
		show_help();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* parameter */
			switch (argv[i][1]) {

				case 'h':
					show_help();
					exit(1);
					break;

				case 'z':
					i++;
					if (i < argc) {
						g->font_size = atoi(argv[i]);
					} else {
						fprintf(stdout,"Insufficient parameters; -z <font size pts>\n");
						exit(1);
					}
					break;

				case 'p':
					/*
					 * com port can be multiple things in linux
					 * such as /dev/ttySx or /dev/ttyUSBxx
					 */
					i++;
					if (i < argc) {
						g->device = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -p <usb TMC port ie, /dev/usbtmc2>\n");
						exit(1);
					}
					break;

				case 'o':
					/* 
					 * output file where this program will put the text
					 * line containing the information FlexBV will want 
					 *
					 */
					i++;
					if (i < argc) {
						g->output_file = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -o <output file>\n");
						exit(1);
					}
					break;

				case 'd': g->debug = 1; break;

				case 'q': g->quiet = 1; break;

				case 'v':
							 fprintf(stdout,"Build %d\r\n", BUILD_VER);
							 exit(0);
							 break;

				case 't':
							 i++;
							 g->interval = atoi(argv[i]);
							 break;

				case 'c':
							 if (argv[i][2] == 'v') {
								 i++;
								 sscanf(argv[i], "%2hhx%2hhx%2hhx"
										 , &g->font_color_pri.r
										 , &g->font_color_pri.g
										 , &g->font_color_pri.b
										 );

							 } else if (argv[i][2] == 'a') {
								 i++;
								 sscanf(argv[i], "%2hhx%2hhx%2hhx"
										 , &g->font_color_sec.r
										 , &g->font_color_sec.g
										 , &g->font_color_sec.b
										 );

							 } else if (argv[i][2] == 'b') {
								 i++;
								 sscanf(argv[i], "%2hhx%2hhx%2hhx"
										 , &(g->background_color.r)
										 , &(g->background_color.g)
										 , &(g->background_color.b)
										 );

							 }
							 break;

				case 'w':
							 if (argv[i][2] == 'x') {
								 i++;
								 g->wx_forced = atoi(argv[i]);
							 } else if (argv[i][2] == 'y') {
								 i++;
								 g->wy_forced = atoi(argv[i]);
							 }
							 break;

				case 's':
							 i++;
							 g->serial_parameters_string = argv[i];
							 break;

				default: break;
			} // switch
		}
	}

	return 0;
}



/*
 * open_port()
 *
 * The DM3058E is fixed in the 8n1 parameters but the
 * serial speed can vary between 9600-115200
 *
 * No flow control
 *
 * Default is 115200
 *
 *
 */
void open_port( struct glb *g ) {

	struct serial_params_s *s = &(g->serial_params);
	char *p = g->serial_parameters_string;
	char default_params[] = "115200";
	int r; 

	if (!p) p = default_params;

	if (g->debug) fprintf(stderr,"%s:%d: Attempting to open '%s'\n", FL, s->device);
	s->fd = open( s->device, O_RDWR | O_NOCTTY | O_NDELAY );
	if (s->fd <0) {
		perror( s->device );
	}

	fcntl(s->fd,F_SETFL,0);
	tcgetattr(s->fd,&(s->oldtp)); // save current serial port settings 
	tcgetattr(s->fd,&(s->newtp)); // save current serial port settings in to what will be our new settings
	cfmakeraw(&(s->newtp));

	s->newtp.c_cflag = CS8 |  CLOCAL | CREAD ; 

	s->newtp.c_cc[VTIME] = 10;
	s->newtp.c_cc[VMIN] = 0;

	if (strncmp(p, "115200", 6) == 0) s->newtp.c_cflag |= B115200; 
	else if (strncmp(p, "57600", 5) == 0) s->newtp.c_cflag |= B57600;
	else if (strncmp(p, "38400", 5) == 0) s->newtp.c_cflag |= B38400;
	else if (strncmp(p, "19200", 5) == 0) s->newtp.c_cflag |= B19200;
	else if (strncmp(p, "9600", 4) == 0) s->newtp.c_cflag |= B9600;
	else {
		fprintf(stdout,"Invalid serial speed\r\n");
		exit(1);
	}

	//  This meter only accepts 8n1, no flow control

	s->newtp.c_iflag &= ~(IXON | IXOFF | IXANY );

	r = tcsetattr(s->fd, TCSANOW, &(s->newtp));
	if (r) {
		fprintf(stderr,"%s:%d: Error setting terminal (%s)\n", FL, strerror(errno));
		exit(1);
	}

	if (g->debug) fprintf(stderr,"Serial port opened, FD[%d]\n", s->fd);
}


/*
 * data_read()
 *
 * char *b : buffer for data
 * ssize_t s : size of buffer; function returns if size limit is hit
 *
 */
int data_read( glb *g ) {
	int bp = 0;
	ssize_t bytes_read = 0;

	do {
		char temp_char;
		bytes_read = read(g->serial_params.fd, &temp_char, 1);
		if (bytes_read) {
			*(g->bp) = temp_char;
			if (*(g->bp) == '\n')  {
				g->read_state++; // switch to next read state
				*(g->bp) = '\0';
				break;
			}

			if (*(g->bp) != '\r') {
				if (g->debug) fprintf(stderr,"%c", *(g->bp));
				(g->bp)++;
				*(g->bp) = '\0';
				g->bytes_remaining--;
			}
		}
	} while (bytes_read && g->bytes_remaining > 0);

	return bp;
}



/*
 * data_write()
 *		const char *d : pointer to data to write/send
 *		ssize_t s : number of bytes to send
 *
 */
int data_write( glb *g, const char *d, ssize_t s ) { 
	ssize_t sz;

	if (g->debug) fprintf(stderr,"%s:%d: Sending '%s' [%ld bytes]\n", FL, d, s );
	sz = write(g->serial_params.fd, d, s); 
	if (sz < 0) {
		g->error_flag = true;
		fprintf(stdout,"Error sending serial data: %s\n", strerror(errno));
	}

	return sz;
}


/*
 * grab_key()
 *
 * Function sets up a global XGrabKey() and additionally registers
 * for num and cap lock keyboard combinations.
 *
 */
void grab_key(Display* display, Window rootWindow, int keycode, int modifier) {
	XGrabKey(display, keycode, modifier, rootWindow, false, GrabModeAsync, GrabModeAsync);

	if (modifier != AnyModifier) {
		XGrabKey(display, keycode, modifier | Mod2Mask, rootWindow, false, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, keycode, modifier | LockMask, rootWindow, false, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, keycode, modifier | Mod2Mask | LockMask, rootWindow, false, GrabModeAsync, GrabModeAsync);
	}
}


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220307
  Function Name	: main
  Returns Type	: int
  ----Parameter List
  1. int argc,
  2.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int main ( int argc, char **argv ) {

	SDL_Event event;
	SDL_Surface *surface, *surface_2;
	SDL_Texture *texture, *texture_2;

	char linetmp[SSIZE]; // temporary string for building main line of text

	struct glb g;        // Global structure for passing variables around
	char tfn[4096];
	bool quit = false;
	bool paused = false;

	glbs = &g;

	/*
	 * Initialise the global structure
	 */
	init(&g);

	/*
	 * Parse our command line parameters
	 */
	parse_parameters(&g, argc, argv);
	if (g.device == NULL) {
		fprintf(stdout,"Require valid device (ie, -p /dev/usbtmc2 )\nExiting\n");
		exit(1);
	}

	if (g.debug) fprintf(stdout,"START\n");

	g.comms_mode = CMODE_SERIAL;
	g.serial_params.device = g.device;

	/* 
	 * check paramters
	 *
	 */
	if (g.font_size < 10) g.font_size = 10;
	if (g.font_size > 200) g.font_size = 200;

	if (g.output_file) snprintf(tfn,sizeof(tfn),"%s.tmp",g.output_file);


	open_port( &g );

	Display*    dpy     = XOpenDisplay(0);
	Window      root    = DefaultRootWindow(dpy);
	XEvent      ev;
	Window          grab_window     =  root;


	// Shift key = ShiftMask / 0x01
	// CapLocks = LockMask / 0x02
	// Control = ControlMask / 0x04
	// Alt = Mod1Mask / 0x08
	//
	// Numlock = Mod2Mask / 0x10
	// Windows key = Mod4Mask / 0x40
	grab_key(dpy, grab_window, XKeysymToKeycode(dpy,XK_a), Mod4Mask|Mod1Mask);
	grab_key(dpy, grab_window, XKeysymToKeycode(dpy,XK_r), Mod4Mask|Mod1Mask);
	grab_key(dpy, grab_window, XKeysymToKeycode(dpy,XK_v), Mod4Mask|Mod1Mask);
	grab_key(dpy, grab_window, XKeysymToKeycode(dpy,XK_c), Mod4Mask|Mod1Mask);
	grab_key(dpy, grab_window, XKeysymToKeycode(dpy,XK_d), Mod4Mask|Mod1Mask);
	grab_key(dpy, grab_window, XKeysymToKeycode(dpy,XK_f), Mod4Mask|Mod1Mask);
	XSelectInput(dpy, root, KeyPressMask);


	/*
	 * Setup SDL2 and fonts
	 *
	 */

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();
	TTF_Font *font = TTF_OpenFont("RobotoMono-Regular.ttf", g.font_size);
	TTF_Font *font_small = TTF_OpenFont("RobotoMono-Regular.ttf", g.font_size/2);

	/*
	 * Get the required window size.
	 *
	 * Parameters passed can override the font self-detect sizing
	 *
	 */
	TTF_SizeText(font, " 00.0000V DCAC ", &g.window_width, &g.window_height);
	g.window_height *= 1.85;

	if (g.wx_forced) g.window_width = g.wx_forced;
	if (g.wy_forced) g.window_height = g.wy_forced;

	SDL_Window *window = SDL_CreateWindow("DM3058E", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g.window_width, g.window_height, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!font) {
		fprintf(stderr,"Error trying to open font :( \r\n");
		exit(1);
	}
	SDL_RendererInfo info;
	SDL_GetRendererInfo( renderer, &info );
	fprintf(stderr,"Renderer Information --\n"\
			"Name: %s\n"\
			"Flags: %lX\n"\
			"%s%s%s%s\n"
			"---\n"\
			, info.name
			, info.flags
			, info.flags&SDL_RENDERER_SOFTWARE?"Software":""
			, info.flags&SDL_RENDERER_ACCELERATED?"Accelerated":""
			, info.flags&SDL_RENDERER_PRESENTVSYNC?"Vsync Sync":""
			, info.flags&SDL_RENDERER_TARGETTEXTURE?"Target texture supported":""
			);

	/* Select the color for drawing. It is set to red here. */
	SDL_SetRenderDrawColor(renderer, g.background_color.r, g.background_color.g, g.background_color.b, 255 );

	/* Clear the entire screen to our selected color. */
	SDL_RenderClear(renderer);

	/*
	 *
	 * Parent will terminate us... else we'll become a zombie
	 * and hope that the almighty PID 1 will reap us
	 *
	 */
	char line1[1024];
	char line2[1024];

	while (!quit) {

		if (!paused && !quit) {
			if (XCheckMaskEvent(dpy, KeyPressMask, &ev)) {
				KeySym ks;
				if (g.debug) fprintf(stderr,"Keypress event %X\n", ev.type);
				switch (ev.type) {
					case KeyPress:
						//					ks = XKeycodeToKeysym(dpy,ev.xkey.keycode,0);
						ks = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
						if (g.debug) fprintf(stderr,"Hot key pressed %X => %lx!\n", ev.xkey.keycode, ks);
						if (g.read_state != READSTATE_NONE && g.read_state != READSTATE_DONE) {
							data_read( &g );
						}
						g.read_state=READSTATE_NONE;  //TO PREVENT NEXT MEAS COMMAND TO SWITCH THE RANGE BACK 
						switch (ks) {
							case XK_r:
								data_write( &g, mmodes[MMODES_RES].query, strlen(mmodes[MMODES_RES].query) );
								break;
							case XK_v:
								data_write( &g, mmodes[MMODES_VOLT_DC].query, strlen(mmodes[MMODES_VOLT_DC].query) );
								break;
							case XK_a:
								data_write( &g, mmodes[MMODES_VOLT_AC].query, strlen(mmodes[MMODES_VOLT_AC].query) );
								break;
							case XK_c:
								data_write( &g, mmodes[MMODES_CONT].query, strlen(mmodes[MMODES_CONT].query) );
								break;
							case XK_d:
								data_write( &g, mmodes[MMODES_DIOD].query, strlen(mmodes[MMODES_DIOD].query) );
								break;
							case XK_f:
								data_write( &g, mmodes[MMODES_CAP].query, strlen(mmodes[MMODES_CAP].query) );
								break;
							default:
								break;
						} // keycode
						
						break;
						
					default:
						break;
				}
			} // check maskpp
		}

		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_q) {
						////data_write( &g, SCPI_LOCAL, strlen(SCPI_LOCAL) );  //RIGOL DOESNT SUPPORT THAT
						quit = true;
					}
					if (event.key.keysym.sym == SDLK_p) {
						paused ^= 1;
					    g.read_state=READSTATE_NONE;  //TO PREVENT NEXT MEAS COMMAND TO SWITCH THE RANGE BACK 
						
						////if (paused == true) data_write( &g, SCPI_LOCAL, strlen(SCPI_LOCAL) ); //RIGOL DOESNT SUPPORT THAT
					}
					break;
				case SDL_QUIT:
					quit = true;
					break;
			}
		}

		linetmp[0] = '\0';


		if (!paused && !quit) {

			if (g.read_state != READSTATE_NONE && g.read_state != READSTATE_DONE) {
				data_read( &g );
			}

			switch (g.read_state) {
				case READSTATE_NONE:
					tcflush(g.serial_params.fd, TCIOFLUSH); // clear buffer TO PREVENT NEX READ ERROR
				case READSTATE_DONE:
					data_write( &g, SCPI_MEAS, strlen(SCPI_MEAS));
					g.bp = g.read_buffer; *(g.bp) = '\0'; g.bytes_remaining = READ_BUF_SIZE;
					//g.read_state = READSTATE_READING_FUNCTION;
					g.read_state = READSTATE_READING_MEASURE;
					break;
					
				case READSTATE_FINISHED_MEASURE:
					if (strcmp(g.read_buffer, "FALSE")==0) {
							if (g.debug) fprintf(stderr,"%s: NO NEW MEASURMENT COMPLETE\n", g.read_buffer);
							data_write( &g, SCPI_MEAS, strlen(SCPI_MEAS));
							g.bp = g.read_buffer; *(g.bp) = '\0'; g.bytes_remaining = READ_BUF_SIZE;
							g.read_state = READSTATE_READING_MEASURE;
						}
					if (strcmp(g.read_buffer, "TRUE")==0) {
							if (g.debug) fprintf(stderr,"%s: WE HAVE A NEW MEASUREMENT\n", g.read_buffer);
							data_write( &g, SCPI_FUNC, strlen(SCPI_FUNC));
							g.bp = g.read_buffer; *(g.bp) = '\0'; g.bytes_remaining = READ_BUF_SIZE;
							g.read_state = READSTATE_READING_FUNCTION;
					}
					break;
					
				case READSTATE_FINISHED_FUNCTION:
					// check the value of the buffer and determine
					// which mode-index (mi) we need for later --- idiot!
					//
					int mi;
					for (mi = 0; mi < MMODES_MAX; mi++) {
						if (strcmp(g.read_buffer, mmodes[mi].scpi)==0) {
							if (g.debug) fprintf(stderr,"%s:%d: HIT on '%s' index %d\n", FL, g.read_buffer, mi);
							break;
						}
					}

					if (mi == MMODES_MAX) {
						fprintf(stderr,"%s:%d: Unknown mode '%s'\n", FL, g.read_buffer);
						continue;
					}

					g.mode_index = mi;

					data_write( &g, mmodes[mi].query, strlen(mmodes[mi].query) );
					g.read_state = READSTATE_READING_VAL;
					g.bp = g.read_buffer; *(g.bp) = '\0'; g.bytes_remaining = READ_BUF_SIZE;
					break;

				case READSTATE_FINISHED_VAL:
					g.v = strtod(g.read_buffer, NULL);
					snprintf(g.value, sizeof(g.value), "%f", g.v);
					if(strcmp(mmodes[mi].range,SKIP)==0)
						g.read_state = READSTATE_FINISHED_ALL;
					else
					{
						data_write( &g, mmodes[mi].range, strlen(mmodes[mi].range) );
						g.read_state = READSTATE_READING_RANGE;
						g.bp = g.read_buffer; *(g.bp) = '\0'; g.bytes_remaining = READ_BUF_SIZE;
					}
					break;

				case READSTATE_FINISHED_RANGE:
					snprintf(g.range, sizeof(g.range), "%s", g.read_buffer);
					//if (g.mode_index == MMODES_CONT) { 
						//g.bp = g.read_buffer; *(g.bp) = '\0'; g.bytes_remaining = READ_BUF_SIZE;
						//data_write( &g, SCPI_CONT_THRESHOLD, strlen(SCPI_CONT_THRESHOLD) );
						//g.read_state = READSTATE_READING_CONTLIMIT;
					//} else {
						g.read_state = READSTATE_FINISHED_ALL;
					//}
					break;

				case READSTATE_FINISHED_CONTLIMIT:
					g.cont_threshold = strtol(g.read_buffer, NULL, 10);
					g.read_state = READSTATE_FINISHED_ALL;
					break;

				case READSTATE_ERROR:
				default:
					snprintf(g.range,sizeof(g.range),"---");
					snprintf(g.value,sizeof(g.value),"---");
					snprintf(g.func,sizeof(g.func),"no data, check port");
					fprintf(stderr,"default readstate reached, error!\n");
					g.read_state = READSTATE_FINISHED_ALL;
					break;
			} // switch readstate

			if (g.read_state == READSTATE_FINISHED_ALL) {
				g.read_state = READSTATE_DONE;
				switch (g.mode_index) {
				
					case MMODES_VOLT_DC:
						if (strcmp(g.range,"0")==0) { 
							snprintf(g.value,sizeof(g.value),"% 07.3f mV DC", g.v *1000.0);
							snprintf(g.range,sizeof(g.range),"200mV");
						}
						else if (strcmp(g.range, "1")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.5f V DC", g.v);
							snprintf(g.range,sizeof(g.range),"2V");
						}
						else if (strcmp(g.range, "2")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.4f V DC", g.v);
							snprintf(g.range,sizeof(g.range),"20V");
						}
						else if (strcmp(g.range, "3")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.3f V DC", g.v);
							snprintf(g.range,sizeof(g.range),"200V");
						}
						else if (strcmp(g.range, "4")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.2f V DC", g.v);
							snprintf(g.range,sizeof(g.range),"1000V");
						}
						break;

					case MMODES_VOLT_AC:
						if (strcmp(g.range,"0")==0) { 
							snprintf(g.value,sizeof(g.value),"% 07.3f mV AC", g.v *1000.0);
							snprintf(g.range,sizeof(g.range),"200mV");
						}
						else if (strcmp(g.range, "1")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.5f V AC", g.v);
							snprintf(g.range,sizeof(g.range),"2V");
						}
						else if (strcmp(g.range, "2")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.4f V AC", g.v);
							snprintf(g.range,sizeof(g.range),"20V");
						}
						else if (strcmp(g.range, "3")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.3f V AC", g.v);
							snprintf(g.range,sizeof(g.range),"200V");
						}
						else if (strcmp(g.range, "4")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.2f V AC", g.v);
							snprintf(g.range,sizeof(g.range),"750V");
						}
						break;

					case MMODES_CURR_DC:
						if (strcmp(g.range,"0")==0) { 
							snprintf(g.value,sizeof(g.value),"% 07.2f uA DC", g.v *1000.0);
							snprintf(g.range,sizeof(g.range),"20uA");
						}
						else if (strcmp(g.range, "1")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.4f mA DC", g.v);
							snprintf(g.range,sizeof(g.range),"2mA");
						}
						else if (strcmp(g.range, "2")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.4f mA DC", g.v);
							snprintf(g.range,sizeof(g.range),"20mA");
						}
						else if (strcmp(g.range, "3")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.2f mA DC", g.v);
							snprintf(g.range,sizeof(g.range),"200mA");
						}
						else if (strcmp(g.range, "4")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.1f A DC", g.v);
							snprintf(g.range,sizeof(g.range),"2A");
						}
						else if (strcmp(g.range, "5")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.1f A DC", g.v);
							snprintf(g.range,sizeof(g.range),"10A");
						}
						break;
					case MMODES_CURR_AC:
						if (strcmp(g.range,"0")==0) { 
							snprintf(g.value,sizeof(g.value),"% 07.2f mA AC", g.v *1000.0);
							snprintf(g.range,sizeof(g.range),"20mA");
						}
						else if (strcmp(g.range, "1")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.4f mA AC", g.v);
							snprintf(g.range,sizeof(g.range),"200mA");
						}
						else if (strcmp(g.range, "2")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.4f A AC", g.v);
							snprintf(g.range,sizeof(g.range),"2A");
						}
						else if (strcmp(g.range, "3")==0) { 
							snprintf(g.value, sizeof(g.value), "% 07.2f A AC", g.v);
							snprintf(g.range,sizeof(g.range),"10A");
						}
						
					case MMODES_RES:
					case MMODES_FRES:	
						if (strcmp(g.range,"0")==0) { 
							snprintf(g.value,sizeof(g.value),"%06.3f %s", g.v, oo);
							snprintf(g.range,sizeof(g.range),"200%s",oo); }
						else if (strcmp(g.range, "1")==0){ 
							snprintf(g.value, sizeof(g.value), "%06.5f k%s", g.v /1000, oo);
							snprintf(g.range,sizeof(g.range),"2K%s",oo); }
						else if (strcmp(g.range, "2")==0){ 
							snprintf(g.value, sizeof(g.value), "%06.4f k%s", g.v /1000, oo);
							snprintf(g.range,sizeof(g.range),"20K%s",oo); }
						else if (strcmp(g.range, "3")==0){ 
							snprintf(g.value, sizeof(g.value), "%06.3f k%s", g.v /1000, oo);
							snprintf(g.range,sizeof(g.range),"200K%s",oo); }
						else if (strcmp(g.range, "4")==0){ 
							snprintf(g.value, sizeof(g.value), "%06.5f M%s", g.v /1000000, oo);
							snprintf(g.range,sizeof(g.range),"1M%s",oo); }
						else if (strcmp(g.range, "5")==0){ 
							snprintf(g.value, sizeof(g.value), "%06.4f M%s", g.v /1000000, oo);
							snprintf(g.range,sizeof(g.range),"10M%s",oo); }
						else if (strcmp(g.range, "6")==0){ 
							snprintf(g.value, sizeof(g.value), "%06.3f M%s", g.v /1000000, oo);
							snprintf(g.range,sizeof(g.range),"100M%s",oo); }	
							
							
						if (g.v >= 9000000000000000.000000) snprintf(g.value, sizeof(g.value), "O.L");
						break;

					case MMODES_CAP:
						if (strcmp(g.range,"0")==0) { 
							snprintf(g.value,sizeof(g.value),"% 6.3f nF", g.v *1E+9 );
							snprintf(g.range,sizeof(g.range),"2nF"); }
						else if (strcmp(g.range, "1")==0){ 
							snprintf(g.value, sizeof(g.value), "% 06.2f nF", g.v *1E+9);
							snprintf(g.range,sizeof(g.range),"20nF"); }
						else if (strcmp(g.range, "2")==0){ 
							snprintf(g.value, sizeof(g.value), "% 06.1f nF", g.v *1E+9);
							snprintf(g.range,sizeof(g.range),"200nF"); }
						else if (strcmp(g.range, "3")==0){ 
							snprintf(g.value, sizeof(g.value), "% 06.3f %sF", g.v *1E+6, uu);
							snprintf(g.range,sizeof(g.range),"2%sF",uu); }
						else if (strcmp(g.range, "4")==0){ 
							snprintf(g.value, sizeof(g.value), "% 06.2f %sF", g.v *1E+6, uu);
							snprintf(g.range,sizeof(g.range),"200%sF",uu); }
						else if (strcmp(g.range, "5")==0){ 
							snprintf(g.value, sizeof(g.value), "% 06.3f %sF", g.v *1E+6, uu);
							snprintf(g.range,sizeof(g.range),"100000%sF",uu); }
						if (g.v >= 51000000000000) snprintf(g.value, sizeof(g.value), "O.L");
						break;


					case MMODES_CONT:
						{ 
							if (g.v > g.cont_threshold) {
								if (g.v > 1000) g.v = 999.9;
								snprintf(g.value, sizeof(g.value), "OPEN [%05.1f%s]", g.v, oo);
							}
							else {
								snprintf(g.value, sizeof(g.value), "SHRT [%05.1f%s]", g.v, oo);
							}
							snprintf(g.range,sizeof(g.range),"Threshold: %d%s", g.cont_threshold, oo);
						}
						break;

					case MMODES_DIOD:
						{ 
							if (g.v > 9.999) {
								snprintf(g.value, sizeof(g.value), "OPEN / OL");
							} else {
								snprintf(g.value, sizeof(g.value), "%06.4f V", g.v);
							}
							snprintf(g.range,sizeof(g.range),"None");
						}
						break;


				}
				snprintf(line1, sizeof(line1), "%s", g.value);
				snprintf(line2, sizeof(line2), "%s, %s", mmodes[g.mode_index].label, g.range);
				if (g.debug) fprintf(stderr,"Value:%f Range: %s\n", g.v, g.range);

			}
		} else if ( paused ) {
			snprintf(line1, sizeof(line1),"Paused");
			snprintf(line2, sizeof(line2),"Press p");
		}
		/*
		 *
		 * END OF DATA ACQUISITION
		 *
		 */



		{
			/*
			 * Rendering
			 *
			 *
			 */
			int texW = 0;
			int texH = 0;
			int texW2 = 0;
			int texH2 = 0;
			SDL_RenderClear(renderer);
			surface = TTF_RenderUTF8_Blended(font, line1, g.font_color_pri);
			texture = SDL_CreateTextureFromSurface(renderer, surface);
			SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
			SDL_Rect dstrect = { 0, 0, texW, texH };
			SDL_RenderCopy(renderer, texture, NULL, &dstrect);

			surface_2 = TTF_RenderUTF8_Blended(font_small, line2, g.font_color_sec);
			texture_2 = SDL_CreateTextureFromSurface(renderer, surface_2);
			SDL_QueryTexture(texture_2, NULL, NULL, &texW2, &texH2);
			dstrect = { 0, texH -(texH /5), texW2, texH2 };
			SDL_RenderCopy(renderer, texture_2, NULL, &dstrect);

			SDL_RenderPresent(renderer);

			SDL_DestroyTexture(texture);
			SDL_FreeSurface(surface);
			if (1) {
				SDL_DestroyTexture(texture_2);
				SDL_FreeSurface(surface_2);
			}

			if (g.error_flag) {
				sleep(1);
			} else {
				usleep(g.interval);
			}
		}


		if (g.output_file) {
			/*
			 * Only write the file out if it doesn't
			 * exist. 
			 *
			 */
			if (!fileExists(g.output_file)) {
				FILE *f;
				if (g.debug) fprintf(stderr,"%s:%d: output filename = %s\r\n", FL, g.output_file);
				f = fopen(tfn,"w");
				if (f) {
					fprintf(f,"%s", linetmp);
					if (g.debug) fprintf(stderr,"%s:%d: %s => %s\r\n", FL, linetmp, tfn);
					fclose(f);
					rename(tfn, g.output_file);
				}
			}
		}

	} // while(1)

	if (g.comms_mode == CMODE_USB) {
		close(g.usb_fhandle);
	}

	XCloseDisplay(dpy);

	TTF_CloseFont(font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();

	return 0;

}
