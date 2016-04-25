//cs335
//
//program: rainforest
//author:  Gordon Griesel
//date:    2013 to present
//
//This program demonstrates the use of OpenGL and XWindows
//
//Texture maps are displayed.
//Press B to see bigfoot roaming his forest.
//
//The rain builds up like snow on the ground.
//Fix it by removing each raindrop for the rain linked list.
//look for the 
//
//
//
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <X11/Xlib.h>
//#include <X11/Xutil.h>
//#include <GL/gl.h>
//#include <GL/glu.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "log.h"
#include "ppm.h"
extern "C" {
#include "fonts.h"
}

#define WINDOW_WIDTH  2000
#define WINDOW_HEIGHT 1000
#define SEGMENTS 60
#define ADJUST (3.14159 * 2.0) / (float)SEGMENTS
#define MAX_PARTICLES 5000
#define GRAVITY 0.1

int set = 0, direction = -1, jump = 0, counter = 0;
int box_x = 400, box_y = 60, box_length = 40, val = 0,
    sprite_x = 80, sprite_y = 41;


//defined types
typedef double Flt;
typedef double Vec[3];
typedef Flt	Matrix[4][4];
double backgroundx = 0;

//macros
#define rnd() (((Flt)rand())/(Flt)RAND_MAX)
#define random(a) (rand()%a)
#define MakeVector(x, y, z, v) (v)[0]=(x),(v)[1]=(y),(v)[2]=(z)
#define VecCopy(a,b) (b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2]
#define VecDot(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VecSub(a,b,c) (c)[0]=(a)[0]-(b)[0]; \
			     (c)[1]=(a)[1]-(b)[1]; \
(c)[2]=(a)[2]-(b)[2]
//constants
const float timeslice = 1.0f;
const float gravity = -0.2f;
#define ALPHA 1

//X Windows variables
Display *dpy;
Window win;


//-----------------------------------------------------------------------------
//Setup timers
const double physicsRate = 1.0 / 30.0;
const double oobillion = 1.0 / 1e9;
struct timespec timeStart, timeCurrent;
struct timespec timePause;
double physicsCountdown=0.0;
double timeSpan=0.0;
unsigned int upause=0;
double timeDiff(struct timespec *start, struct timespec *end) {
	return (double)(end->tv_sec - start->tv_sec ) +
		(double)(end->tv_nsec - start->tv_nsec) * oobillion;
}
void timeCopy(struct timespec *dest, struct timespec *source) {
	memcpy(dest, source, sizeof(struct timespec));
}
//-----------------------------------------------------------------------------


int done=0;
int xres=800, yres=600;

typedef struct t_background {
	Vec pos;
	Vec vel;
} Background;
Background background;

typedef struct t_bigfoot {
	Vec pos;
	Vec vel;
} Bigfoot;
Bigfoot bigfoot;

Ppmimage *bigfootImage=NULL;
Ppmimage *forestImage=NULL;
Ppmimage *forestTransImage=NULL;
Ppmimage *umbrellaImage=NULL;
GLuint bigfootTexture;
GLuint silhouetteTexture;
GLuint forestTexture;
GLuint forestTransTexture;
GLuint umbrellaTexture;
int showBigfoot=0;
int forest=1;
int silhouette=1;
int trees=1;
int showRain=0;
//
typedef struct t_raindrop {
	int type;
	int linewidth;
	int sound;
	Vec pos;
	Vec lastpos;
	Vec vel;
	Vec maxvel;
	Vec force;
	float length;
	float color[4];
	struct t_raindrop *prev;
	struct t_raindrop *next;
} Raindrop;
Raindrop *ihead=NULL;
int ndrops=1;
int totrain=0;
int maxrain=0;
void deleteRain(Raindrop *node);
void cleanupRaindrops(void);
//
#define UMBRELLA_FLAT  0
#define UMBRELLA_ROUND 1
typedef struct t_umbrella {
	int shape;
	Vec pos;
	Vec lastpos;
	float width;
	float width2;
	float radius;
} Umbrella;
Umbrella umbrella;
int showUmbrella=0;
int deflection=0;

//Structures
struct Vecs {
	float x, y, z;
};

struct Shape {
	float width, height;
	float radius;
	Vecs center;
};

struct Particle {
	Shape s;
	Vecs velocity;
};

struct Game {
	Shape box[5];
	Shape circle[2];
	Particle particle[MAX_PARTICLES];
	int n;
	int lastMousex;
	int lastMousey;
};

//function prototypes
void initXWindows(void);
void initOpengl(void);
void cleanupXWindows(void);
void checkResize(XEvent *e);
void checkMouse(XEvent *e);
void checkKeys(XEvent *e);
void init();
void physics(void);
void render(Game *game);
int check_Gamekeys(XEvent *e, Game *game);
void movement(Game *game);

int main(void)
{
	//int keys = 0;
	Game game;
	game.n=0;	

	//declare a box shape
	int x = 400, y = 10, length = 4000;
	//declare a box shape
	game.box[0].width = length;
	game.box[0].height = 10;
	game.box[0].center.x = x;
	game.box[0].center.y = y;
	game.circle[0].center.x = 10;
	game.circle[0].center.y = -20;
	game.circle[0].radius = 100;


	game.box[2].width = 10;
	game.box[2].height = 20;
	game.box[2].center.x = sprite_x;
	game.box[2].center.y = sprite_y;

	logOpen();
	initXWindows();
	initOpengl();
	init();
	clock_gettime(CLOCK_REALTIME, &timePause);
	clock_gettime(CLOCK_REALTIME, &timeStart);
	while (!done) {
		while (XPending(dpy)) {
			XEvent e;
			XNextEvent(dpy, &e);
			checkResize(&e);
			checkMouse(&e);
			checkKeys(&e);
			//keys = check_Gamekeys(&e, &game);
		}
		//
		//Below is a process to apply physics at a consistent rate.
		//1. Get the time right now.
		clock_gettime(CLOCK_REALTIME, &timeCurrent);
		//2. How long since we were here last?
		timeSpan = timeDiff(&timeStart, &timeCurrent);
		//3. Save the current time as our new starting time.
		timeCopy(&timeStart, &timeCurrent);
		//4. Add time-span to our countdown amount.
		physicsCountdown += timeSpan;
		//5. Has countdown gone beyond our physics rate? 
		//       if yes,
		//           In a loop...
		//              Apply physics
		//              Reducing countdown by physics-rate.
		//              Break when countdown < physics-rate.
		//       if no,
		//           Apply no physics this frame.
		while (physicsCountdown >= physicsRate) {
			//6. Apply physics
			physics();
			//7. Reduce the countdown by our physics-rate
			physicsCountdown -= physicsRate;
		}
		//Always render every frame.
		movement(&game);
		render(&game);
		glXSwapBuffers(dpy, win);
	}
	cleanupXWindows();
	cleanup_fonts();
	logClose();
	return 0;
}

void makeParticle(Game *game, int x, int y) {
	if (game->n >= MAX_PARTICLES)
		return;
	//position of particle
	Particle *p = &game->particle[game->n];
	p->s.center.x = rand() % 51 + 380;
	p->s.center.y = y;
	p->velocity.y = rnd()*0.1 - 0.5;
	p->velocity.x = direction;
	game->n++;
}


void cleanupXWindows(void)
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

void setTitle(void)
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "CS335 - OpenGL Animation Template Under XWindows");
}

void setupScreenRes(const int w, const int h)
{
	xres = w;
	yres = h;
}



void initXWindows(void)
{
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	//GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, None };
	XSetWindowAttributes swa;

	setupScreenRes(640, 480);
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		printf("\n\tcannot connect to X server\n\n");
		exit(EXIT_FAILURE);
	}
	Window root = DefaultRootWindow(dpy);
	XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
	if (vi == NULL) {
		printf("\n\tno appropriate visual found\n\n");
		exit(EXIT_FAILURE);
	} 
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
		StructureNotifyMask | SubstructureNotifyMask;
	win = XCreateWindow(dpy, root, 0, 0, xres, yres, 0,
			vi->depth, InputOutput, vi->visual,
			CWColormap | CWEventMask, &swa);
	GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
	setTitle();
}

void reshapeWindow(int width, int height)
{
	//window has been resized.
	setupScreenRes(width, height);
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, xres, 0, yres, -1, 1);
	setTitle();
}

unsigned char *buildAlphaData(Ppmimage *img)
{
	//add 4th component to RGB stream...
	int i;
	int a,b,c;
	unsigned char *newdata, *ptr;
	unsigned char *data = (unsigned char *)img->data;
	newdata = (unsigned char *)malloc(img->width * img->height * 4);
	ptr = newdata;
	for (i=0; i<img->width * img->height * 3; i+=3) {
		a = *(data+0);
		b = *(data+1);
		c = *(data+2);
		*(ptr+0) = a;
		*(ptr+1) = b;
		*(ptr+2) = c;
		//get largest color component...
		//*(ptr+3) = (unsigned char)((
		//		(int)*(ptr+0) +
		//		(int)*(ptr+1) +
		//		(int)*(ptr+2)) / 3);
		//d = a;
		//if (b >= a && b >= c) d = b;
		//if (c >= a && c >= b) d = c;
		//*(ptr+3) = d;
		*(ptr+3) = (a|b|c);
		ptr += 4;
		data += 3;
	}
	return newdata;
}

void initOpengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, xres, yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//This sets 2D mode (no perspective)
	glOrtho(0, xres, 0, yres, -1, 1);

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);

	//Clear the screen
	glClearColor(1.0, 1.0, 1.0, 1.0);
	//glClear(GL_COLOR_BUFFER_BIT);
	//Do this to allow fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();
	//
	//load the images file into a ppm structure.
	//
	bigfootImage     = ppm6GetImage("./images/bigfoot.ppm");
	forestImage      = ppm6GetImage("./images/gamebackground.ppm");
	forestTransImage = ppm6GetImage("./images/forestTrans.ppm");
	umbrellaImage    = ppm6GetImage("./images/umbrella.ppm");
	//
	//create opengl texture elements
	glGenTextures(1, &bigfootTexture);
	glGenTextures(1, &silhouetteTexture);
	glGenTextures(1, &forestTexture);
	glGenTextures(1, &umbrellaTexture);
	//-------------------------------------------------------------------------
	//bigfoot
	//
	int w = bigfootImage->width;
	int h = bigfootImage->height;
	//
	glBindTexture(GL_TEXTURE_2D, bigfootTexture);
	//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
			GL_RGB, GL_UNSIGNED_BYTE, bigfootImage->data);
	//-------------------------------------------------------------------------
	//
	//silhouette
	//this is similar to a sprite graphic
	//
	glBindTexture(GL_TEXTURE_2D, silhouetteTexture);
	//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	//
	//must build a new set of data...
	unsigned char *silhouetteData = buildAlphaData(bigfootImage);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, silhouetteData);
	free(silhouetteData);
	//glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
	//	GL_RGB, GL_UNSIGNED_BYTE, bigfootImage->data);
	//-------------------------------------------------------------------------
	//
	//umbrella
	//
	glBindTexture(GL_TEXTURE_2D, umbrellaTexture);
	//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	//
	//must build a new set of data...
	silhouetteData = buildAlphaData(umbrellaImage);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, silhouetteData);
	free(silhouetteData);
	//glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
	//	GL_RGB, GL_UNSIGNED_BYTE, bigfootImage->data);
	//-------------------------------------------------------------------------
	//
	//forest
	int forest_w = WINDOW_WIDTH * 2;
	int forest_h = WINDOW_HEIGHT;
	glBindTexture(GL_TEXTURE_2D, forestTexture);
	//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3,
			forestImage->width, forestImage->height,
			0, GL_RGB, GL_UNSIGNED_BYTE, forestImage->data);
	//-------------------------------------------------------------------------
	//
	//forest transparent part
	//
	glBindTexture(GL_TEXTURE_2D, forestTransTexture);
	//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	//
	//must build a new set of data...
	w = forestTransImage->width;
	h = forestTransImage->height;
	unsigned char *ftData = buildAlphaData(forestTransImage);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, ftData);
	free(ftData);
	//glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
	//GL_RGB, GL_UNSIGNED_BYTE, bigfootImage->data);
	//-------------------------------------------------------------------------
}

void checkResize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != xres || xce.height != yres) {
		//Window size did change.
		reshapeWindow(xce.width, xce.height);
	}
}

void initSounds(void)
{
	//You may add sound here for some extra credit.
	//Fmod is not allowed.
	//OpenAL sound only.
	//Look for the openalTest folder under code.








}

void init() {
	umbrella.pos[0] = 220.0;
	umbrella.pos[1] = (double)(yres-200);
	VecCopy(umbrella.pos, umbrella.lastpos);
	umbrella.width = 200.0;
	umbrella.width2 = umbrella.width * 0.5;
	umbrella.radius = (float)umbrella.width2;
	umbrella.shape = UMBRELLA_FLAT;
	MakeVector(-150.0,180.0,0.0, bigfoot.pos);
	MakeVector(6.0,0.0,0.0, bigfoot.vel);
}

void checkMouse(XEvent *e)
{
	//Did the mouse move?
	//Was a mouse button clicked?
	static int savex = 0;
	static int savey = 0;
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button is down
		}
		if (e->xbutton.button==3) {
			//Right button is down
		}
	}
	if (savex != e->xbutton.x || savey != e->xbutton.y) {
		//Mouse moved
		savex = e->xbutton.x;
		savey = e->xbutton.y;
	}
}

/*int check_Gamekeys(XEvent *e, Game *game) {
	if (e->type == KeyPress) {
		int key = XLookupKeysym(&e->xkey, 0);
		if (key == XK_Escape) {
			return 1;
		}
		//You may check other keys here
		if (key == XK_s) {
			set ^= 1 ;
			return 0; 
		}

		if (key == XK_Up) {
		    	if(jump == 0)
				jump = 1;
			return 0; 
		}

	}
	return 0;
}*/

void checkKeys(XEvent *e)
{
	//keyboard input?
	static int shift=0;
	int key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyRelease) {
		if (key == XK_Shift_L || key == XK_Shift_R)
			shift=0;
		return;
	}
	if (e->type == KeyPress) {
		if (key == XK_Shift_L || key == XK_Shift_R) {
			shift=1;
			return;
		}
	} else {
		return;
	}
	switch(key) {
		case XK_b:
			showBigfoot ^= 1;
			if (showBigfoot) {
				bigfoot.pos[0] = -250.0;
			}
			break;
		case XK_d:
			deflection ^= 1;
			break;
		case XK_f:
			forest ^= 1;
			break;
		case XK_s:
			silhouette ^= 1;
			printf("silhouette: %i\n",silhouette);
			break;
		case XK_t:
			trees ^= 1;
			break;
		case XK_u:
			showUmbrella ^= 1;
			break;
		case XK_p:
			umbrella.shape ^= 1;
			break;
		case XK_r:
			showRain ^= 1;
			//if (!show_rain)
			//	cleanup_raindrops();
			break;
		case XK_Left:
			VecCopy(umbrella.pos, umbrella.lastpos);
			umbrella.pos[0] -= 10.0;
			break;
		case XK_Right:
			VecCopy(umbrella.pos, umbrella.lastpos);
			umbrella.pos[0] += 10.0;
			break;
		case XK_Down:
			VecCopy(umbrella.pos, umbrella.lastpos);
			umbrella.pos[1] -= 10.0;
			break;
		case XK_equal:
			if (++ndrops > 40)
				ndrops=40;
			break;
		case XK_minus:
			if (--ndrops < 0)
				ndrops = 0;
			break;
		case XK_n:
			break;
		case XK_w:
			if (shift) {
				//shrink the umbrella
				umbrella.width *= (1.0 / 1.05);
			} else {
				//enlarge the umbrella
				umbrella.width *= 1.05;
			}
			//half the width
			umbrella.width2 = umbrella.width * 0.5;
			umbrella.radius = (float)umbrella.width2;
			break;
		case XK_Up:
		    	if(jump == 0)
				jump = 1;
			break;
		case XK_Escape:
			done=1;
			break;
	}
}

Flt VecNormalize(Vec vec)
{
	Flt len, tlen;
	Flt xlen = vec[0];
	Flt ylen = vec[1];
	Flt zlen = vec[2];
	len = xlen*xlen + ylen*ylen + zlen*zlen;
	if (len == 0.0) {
		MakeVector(0.0,0.0,1.0,vec);
		return 1.0;
	}
	len = sqrt(len);
	tlen = 1.0 / len;
	vec[0] = xlen * tlen;
	vec[1] = ylen * tlen;
	vec[2] = zlen * tlen;
	return(len);
}

void cleanupRaindrops(void)
{
	Raindrop *s;
	while (ihead) {
		s = ihead->next;
		free(ihead);
		ihead = s;
	}
	ihead=NULL;
}

void deleteRain(Raindrop *node)
{
	//remove a node from linked list
	//this line keeps the compiler happy for now.
	if (node) {}


	//hints:
	//check for some special cases:
	//1. only 1 node in list (it is also the head node)
	//2. node at beginning of list (it is also the head node)
	//3. node at end of list.
	//4. node somewhere else in list.

	//if (node->prev == NULL) <--- node at beginning of list.







	//At the end of this function, free the node's memory,
	//and set the node to NULL.
}

void moveBigfoot()
{
	//move bigfoot...
	int addgrav = 1;
	//Update position
	bigfoot.pos[0] += bigfoot.vel[0];
	bigfoot.pos[1] += bigfoot.vel[1];
	//Check for collision with window edges
	if ((bigfoot.pos[0] < -140.0 && bigfoot.vel[0] < 0.0) ||
			(bigfoot.pos[0] >= (float)xres+140.0 && bigfoot.vel[0] > 0.0)) {
		bigfoot.vel[0] = -bigfoot.vel[0];
		addgrav = 0;
	}
	if ((bigfoot.pos[1] < 150.0 && bigfoot.vel[1] < 0.0) ||
			(bigfoot.pos[1] >= (float)yres && bigfoot.vel[1] > 0.0)) {
		bigfoot.vel[1] = -bigfoot.vel[1];
		addgrav = 0;
	}
	//Gravity
	if (addgrav)
		bigfoot.vel[1] -= 0.75;
}


void createRaindrop(const int n)
{
	//create new rain drops...
	int i;
	for (i=0; i<n; i++) {
		Raindrop *node = (Raindrop *)malloc(sizeof(Raindrop));
		if (node == NULL) {
			Log("error allocating node.\n");
			exit(EXIT_FAILURE);
		}
		node->prev = NULL;
		node->next = NULL;
		node->sound=0;
		node->pos[0] = rnd() * (float)xres;
		node->pos[1] = rnd() * 100.0f + (float)yres;
		VecCopy(node->pos, node->lastpos);
		node->vel[0] = 
			node->vel[1] = 0.0f;
		node->color[0] = rnd() * 0.2f + 0.8f;
		node->color[1] = rnd() * 0.2f + 0.8f;
		node->color[2] = rnd() * 0.2f + 0.8f;
		node->color[3] = rnd() * 0.5f + 0.3f; //alpha
		node->linewidth = random(8)+1;
		//larger linewidth = faster speed
		node->maxvel[1] = (float)(node->linewidth*16);
		node->length = node->maxvel[1] * 0.2f + rnd();
		//put raindrop into linked list
		node->next = ihead;
		if (ihead != NULL)
			ihead->prev = node;
		ihead = node;
		++totrain;
	}
}

void checkRaindrops()
{
	if (random(100) < 50) {
		createRaindrop(ndrops);
	}
	//
	//move rain droplets
	Raindrop *node = ihead;
	while (node) {
		//force is toward the ground
		node->vel[1] += gravity;
		VecCopy(node->pos, node->lastpos);

		//----------------------------------------------------------------
		//The next 2 lines are temporary code just for this assignment.
		//Comment them out, then fix the raindrop delet function.
		//----------------------------------------------------------------
		float test = rnd() * 100.0;
		if (node->pos[1] > test)
		{
			node->pos[0] += node->vel[0] * timeslice;
			node->pos[1] += node->vel[1] * timeslice;
			if (fabs(node->vel[1]) > node->maxvel[1])
				node->vel[1] *= 0.96;
			node->vel[0] *= 0.999;
		}
		//
		node = node->next;
	}
	//
	//check rain droplets
	int n=0;
	node = ihead;
	while (node) {
		n++;
#ifdef USE_SOUND
		if (node->pos[1] < 0.0f) {
			//raindrop hit ground
			if (!node->sound && play_sounds) {
				//small chance that a sound will play
				int r = random(50);
				if (r==1) {
					//play sound here...



				}
				//sound plays once per raindrop
				node->sound=1;
			}
		}
#endif //USE_SOUND
		//collision detection for raindrop on umbrella
		if (showUmbrella) {
			if (umbrella.shape == UMBRELLA_FLAT) {
				if (node->pos[0] >= (umbrella.pos[0] - umbrella.width2) &&
						node->pos[0] <= (umbrella.pos[0] + umbrella.width2)) {
					if (node->lastpos[1] > umbrella.lastpos[1] ||
							node->lastpos[1] > umbrella.pos[1]) {
						if (node->pos[1] <= umbrella.pos[1] ||
								node->pos[1] <= umbrella.lastpos[1]) {
							if (node->linewidth > 1) {
								Raindrop *savenode = node->next;
								deleteRain(node);
								node = savenode;
								continue;
							}
						}
					}
				}
			}
			if (umbrella.shape == UMBRELLA_ROUND) {
				float d0 = node->pos[0] - umbrella.pos[0];
				float d1 = node->pos[1] - umbrella.pos[1];
				float distance = sqrt((d0*d0)+(d1*d1));
				//Log("distance: %f  umbrella.radius: %f\n",
				//							distance,umbrella.radius);
				if (distance <= umbrella.radius &&
						node->pos[1] > umbrella.pos[1]) {
					if (node->linewidth > 1) {
						if (deflection) {
							//deflect raindrop
							double dot;
							Vec v, up = {0,1,0};
							VecSub(node->pos, umbrella.pos, v);
							VecNormalize(v);
							node->pos[0] =
								umbrella.pos[0] + v[0] * umbrella.radius;
							node->pos[1] =
								umbrella.pos[1] + v[1] * umbrella.radius;
							dot = VecDot(v,up);
							dot += 1.0;
							node->vel[0] += v[0] * dot * 1.0;
							node->vel[1] += v[1] * dot * 1.0;
						} else {
							Raindrop *savenode = node->next;
							deleteRain(node);
							node = savenode;
							continue;
						}
					}
				}
			}
		}
		if (node->pos[1] < -20.0f) {
			//rain drop is below the visible area
			Raindrop *savenode = node->next;
			deleteRain(node);
			node = savenode;
			continue;
		}
		node = node->next;
	}
	if (maxrain < n)
		maxrain = n;
}

void physics(void)
{
	if (showBigfoot)
		moveBigfoot();
	if (showRain)
		checkRaindrops();
}

void drawUmbrella(void)
{
	//Log("drawUmbrella()...\n");
	if (umbrella.shape == UMBRELLA_FLAT) {
		glColor4f(1.0f, 0.2f, 0.2f, 0.5f);
		glLineWidth(8);
		glBegin(GL_LINES);
		glVertex2f(umbrella.pos[0]-umbrella.width2, umbrella.pos[1]);
		glVertex2f(umbrella.pos[0]+umbrella.width2, umbrella.pos[1]);
		glEnd();
		glLineWidth(1);
	} else {
		glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
		glPushMatrix();
		glTranslatef(umbrella.pos[0],umbrella.pos[1],umbrella.pos[2]);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.0f);
		glBindTexture(GL_TEXTURE_2D, umbrellaTexture);
		glBegin(GL_QUADS);
		float w = umbrella.width2;
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-w,  w);
		glTexCoord2f(1.0f, 0.0f); glVertex2f( w,  w);
		glTexCoord2f(1.0f, 1.0f); glVertex2f( w, -w);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-w, -w);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_ALPHA_TEST);
		glPopMatrix();
	}
}

void drawRaindrops(void)
{
	//if (ihead) {
	Raindrop *node = ihead;
	while (node) {
		glPushMatrix();
		glTranslated(node->pos[0],node->pos[1],node->pos[2]);
		glColor4fv(node->color);
		glLineWidth(node->linewidth);
		glBegin(GL_LINES);
		glVertex2f(0.0f, 0.0f);
		glVertex2f(0.0f, node->length);
		glEnd();
		glPopMatrix();
		node = node->next;
	}
	//}
	glLineWidth(1);
}

void render(Game *game)
{
	Rect r;

	//Clear the screen
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	//
	//
	//draw a quad with texture
	float wid = 120.0f;
	glColor3f(1.0, 1.0, 1.0);
	if (forest) {
		glBindTexture(GL_TEXTURE_2D, forestTexture);
		glBegin(GL_QUADS);
		/*glTexCoord2f(0.0f, 1.0f); glVertex2i(backgroundx, 0);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(backgroundx, yres);
		glTexCoord2f(1.0f, 0.0f); glVertex2i((xres*2)+backgroundx, yres);
		glTexCoord2f(1.0f, 1.0f); glVertex2i((xres*2)+backgroundx, 0);*/
		glTexCoord2f(0.0f-backgroundx, 1.0f); glVertex2i(0, 0);
		glTexCoord2f(0.0f-backgroundx, 0.0f); glVertex2i(0, yres);
		glTexCoord2f(1.0f-backgroundx, 0.0f); glVertex2i(xres, yres);
		glTexCoord2f(1.0f-backgroundx, 1.0f); glVertex2i(xres, 0);
		glEnd();
	}
	backgroundx-=.005;
	if (showBigfoot) {
		glPushMatrix();
		glTranslatef(bigfoot.pos[0], bigfoot.pos[1], bigfoot.pos[2]);
		if (!silhouette) {
			glBindTexture(GL_TEXTURE_2D, bigfootTexture);
		} else {
			glBindTexture(GL_TEXTURE_2D, silhouetteTexture);
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.0f);
			glColor4ub(255,255,255,255);
		}
		glBegin(GL_QUADS);
		if (bigfoot.vel[0] > 0.0) {
			glTexCoord2f(0.0f, 1.0f); glVertex2i(-wid,-wid);
			glTexCoord2f(0.0f, 0.0f); glVertex2i(-wid, wid);
			glTexCoord2f(1.0f, 0.0f); glVertex2i( wid, wid);
			glTexCoord2f(1.0f, 1.0f); glVertex2i( wid,-wid);
		} else {
			glTexCoord2f(1.0f, 1.0f); glVertex2i(-wid,-wid);
			glTexCoord2f(1.0f, 0.0f); glVertex2i(-wid, wid);
			glTexCoord2f(0.0f, 0.0f); glVertex2i( wid, wid);
			glTexCoord2f(0.0f, 1.0f); glVertex2i( wid,-wid);
		}
		glEnd();
		glPopMatrix();
		//
		if (trees && silhouette) {
			glBindTexture(GL_TEXTURE_2D, forestTransTexture);
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
			glTexCoord2f(0.0f, 0.0f); glVertex2i(0, yres);
			glTexCoord2f(1.0f, 0.0f); glVertex2i(xres, yres);
			glTexCoord2f(1.0f, 1.0f); glVertex2i(xres, 0);
			glEnd();
		}
		glDisable(GL_ALPHA_TEST);
	}


	float w, h;
	//Draw shapes...
	//
	//circle
	Shape *s;
	float rad = 0.0;
	int x = 0, y = 0;
	glColor3ub(90,140,90);
	for (int i = 0; i < SEGMENTS; i++) {
		for (int j = 0; j < 2; j++){
			s = &game->circle[j];
			x = cos(ADJUST) * s->radius + s->center.x;
			y = sin(ADJUST) * s->radius + s->center.y;
			rad += ADJUST;
			glVertex2f(x, y);
		}
	}

	//draw box
	glColor3ub(90,140,90);
	for (int j=0; j<5; j++){
		s = &game->box[j];
		glPushMatrix();
		glTranslatef(s->center.x, s->center.y, s->center.z);
		w = s->width;
		h = s->height;
		glBegin(GL_QUADS);
		glVertex2i(-w,-h);
		glVertex2i(-w, h);
		glVertex2i( w, h);
		glVertex2i( w,-h);
		glEnd();
		glPopMatrix();
	}


	glDisable(GL_TEXTURE_2D);
	//glColor3f(1.0f, 0.0f, 0.0f);
	//glBegin(GL_QUADS);
	//	glVertex2i(10,10);
	//	glVertex2i(10,60);
	//	glVertex2i(60,60);
	//	glVertex2i(60,10);
	//glEnd();
	//return;
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	if (showRain)
		drawRaindrops();
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	//
	if (showUmbrella)
		drawUmbrella();
	glBindTexture(GL_TEXTURE_2D, 0);
	//
	//
	r.bot = yres - 20;
	r.left = 10;
	r.center = 0;
	ggprint8b(&r, 16, 0, "B - Bigfoot");
	ggprint8b(&r, 16, 0, "F - Forest");
	ggprint8b(&r, 16, 0, "S - Silhouette");
	ggprint8b(&r, 16, 0, "T - Trees");
	ggprint8b(&r, 16, 0, "U - Umbrella");
	ggprint8b(&r, 16, 0, "R - Rain");
	ggprint8b(&r, 16, 0, "D - Deflection");
	ggprint8b(&r, 16, 0, "N - Sounds");
}
void movement(Game *game)
{
	Particle *p;

	//declare a box shape
	game->box[1].width = box_length;
	game->box[1].height = 40;
	game->box[1].center.x = box_x -= 3;
	game->box[1].center.y = box_y;

	if (jump > 0) {
		game->box[2].center.y += 2;
		jump++;
		if (jump == 51)
			jump = -50;
	} 
	else {
		if(game->box[2].center.y > sprite_y) {
			game->box[2].center.y -= 2;
			sleep(.8);
			jump ++;
		}
	}
	/*if (val == 0 ) { 
	  box_y += 60;
	  }
	  else { 
	  box_y -= 60;
	  }*/

	if ( game->box[1].center.x < -40)
		box_x = 1500;
	game->lastMousex = 445;
	game->lastMousey = 540;
	if(set){
		for(int i = 0; i < 10; i++)
			makeParticle(game, game->lastMousex, game->lastMousey);
	}
	if (game->n <= 0)
		return;
	int box_num = 5;
	for(int i=0; i<game->n; i++){
		p = &game->particle[i];
		p->s.center.x += p->velocity.x;
		p->s.center.y += p->velocity.y;
		p->velocity.y -= GRAVITY;

		for (int b = 0; b < box_num; b++) {
			Shape *s = &game->box[b];	 
			if (p->s.center.y >= s->center.y - (s->height) &&
					p->s.center.y <= s->center.y + (s->height) &&
					p->s.center.x >= s->center.x - (s->width) &&
					p->s.center.x <= s->center.x + (s->width)) {
				p->velocity.y *= -0.45;
				p->s.center.y = s->center.y + s->height + 0.01;
			}
		}
		//check circle collison
		float d0, d1, dist;
		for (int j = 0; j < 2; j++) {
			d0 = p->s.center.x - game->circle[j].center.x;
			d1 = p->s.center.y - game->circle[j].center.y;
			dist = sqrt(d0*d0 + d1*d1);
			if(dist <= game->circle[j].radius) {
				d0 /= dist;
				d1 /= dist;
				d0 *= game->circle[j].radius * 1.01;
				d1 *= game->circle[j].radius * 1.01;
				p->s.center.x = game->circle[j].center.x + d0;
				p->s.center.y = game->circle[j].center.y + d1;
				p->velocity.x += d0 * 0.02;
				p->velocity.y += d0 * 0.02;
			}
		}
		if (p->s.center.y < 0.0) {
			game->particle[i] = game->particle[game->n-1];
			game->n--;
		}
	}
}
