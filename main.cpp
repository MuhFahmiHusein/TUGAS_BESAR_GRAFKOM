#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 50;
static int viewy = 24;
static int viewz = 80;

float rot = 0;

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainJalan;
Terrain* _terrainAir;

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainJalan;
}

//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 glTranslatef(0.0f, 0.0f, -10.0f);
	 glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	 glRotatef(-_angle, 0.0f, 1.0f, 0.0f);

	 GLfloat ambientColor[] = {0.4f, 0.4f, 0.4f, 1.0f};
	 glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);

	 GLfloat lightColor0[] = {0.6f, 0.6f, 0.6f, 1.0f};
	 GLfloat lightPos0[] = {-0.5f, 0.8f, 0.1f, 0.0f};
	 glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
	 glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
	 */
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}
void marka()
{
    glPushMatrix();
	glColor3f(1.0f, 1.0f, 1.0f);
    glScaled(10.15, 0.1 , 2.5);
    glutSolidCube(0.5f);
    glPopMatrix();
}

void bar() {
//<<<<<<<<<<<<<<<<<  BAR  >>>>>>>>>>>>>>>>>>>>
     //bar bawah
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(-0.3,-1.0,-3.0);
     glScalef(8.0f, 1.5f, 1.0f);
     GLfloat cubebar_diffuse[] = {2.0, 1.0, 0.0, 0.0 };
     GLfloat cubebar_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar_shininess[] = { 100.0 };
     GLfloat cubebar_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //bar atas
     glPushMatrix();
     glColor3f(0,0.0,0.0);
     glTranslatef(-0.05,-0.2,-3.0);
     glScalef(7.5f, 0.2f, 1.2f);
     GLfloat cubebar1_diffuse[] = {0.1, 0.0, 0.0, 0.1 };
     GLfloat cubebar1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar1_shininess[] = { 100.0 };
     GLfloat cubebar1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //bar samping
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(2.7,-1.0,-4.0);
     glScalef(2.0f, 1.5f, 1.0f);
     GLfloat cubebar2_diffuse[] = {0.1, 0.0, 0.0, 0.1 };
     GLfloat cubebar2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar2_shininess[] = { 100.0 };
     GLfloat cubebar2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //papan atas
     glPushMatrix();
     glColor3f(0.0,0.0,0.0);
     glTranslatef(-0.0,2.2,-2.6);
     glScalef(7.8f, 1.2f, 0.1f);
     GLfloat cubebar3_diffuse[] = {0.1, 0.0, 0.0, 0.0};
     GLfloat cubebar3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar3_shininess[] = { 100.0 };
     GLfloat cubebar3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //papan samping
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(3.8,0.3,-3.5);
     glScalef(0.2f, 5.0f, 2.0f);
     GLfloat cubebar4_diffuse[] = {0.1, 0.0, 0.0, 0.0 };
     GLfloat cubebar4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar4_shininess[] = { 100.0 };
     GLfloat cubebar4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar4_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tutup depan
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(2.5,0.5,-2.6);
     glScalef(2.5f, 1.2f, 0.1f);
     GLfloat cubebar5_diffuse[] = {0.0, 0.1, 0.0, 0.1};
     GLfloat cubebar5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar5_shininess[] = { 100.0 };
     GLfloat cubebar5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar5_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar5_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tutup samping
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(1.3,0.5,-3.05);
     glScalef(0.1f, 1.2f, 1.0f);
     GLfloat cubebar6_diffuse[] = {0.0, 0.1, 0.0, 0.1 };
     GLfloat cubebar6_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubebar6_shininess[] = { 100.0 };
     GLfloat cubebar6_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubebar6_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubebar6_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubebar6_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubebar6_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubebar6_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubebar6_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<  TIANG DALAM  >>>>>>>>>>>>>>>>>>>>
     //TIANG 1
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(9.5,0.3,-3.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetd_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetd_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetd_shininess[] = { 100.0 };
     GLfloat cubetd_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetd_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetd_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetd_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetd_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetd_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetd_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //TIANG 2
     glPushMatrix();
     glTranslatef(9.0,0.3,-3.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetd1_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetd1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetd1_shininess[] = { 100.0 };
     GLfloat cubetd1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetd1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetd1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetd1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetd1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetd1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetd1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //TIANG 3
     glPushMatrix();
     glTranslatef(8.5,0.3,-3.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetd2_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetd2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetd2_shininess[] = { 100.0 };
     GLfloat cubetd2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetd2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetd2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetd2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetd2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetd2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetd2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //TIANG 4
     glPushMatrix();
     glTranslatef(8.0,0.3,-3.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetd3_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetd3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetd3_shininess[] = { 100.0 };
     GLfloat cubetd3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetd3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetd3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetd3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetd3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetd3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetd3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //TIANG 5
     glPushMatrix();
     glTranslatef(7.5,0.3,-3.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetd4_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetd4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetd4_shininess[] = { 100.0 };
     GLfloat cubetd4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetd4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetd4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetd4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetd4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetd4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetd4_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //TIANG 6
     glPushMatrix();
     glTranslatef(7.0,0.3,-3.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetd5_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetd5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetd5_shininess[] = { 100.0 };
     GLfloat cubetd5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetd5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetd5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetd5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetd5_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetd5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetd5_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<  ATAP  >>>>>>>>>>>>>>>>>>>>
     //atap
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(2.8,3.0,0.0);
     glScalef(15.0f, 0.4f, 15.0f);
     GLfloat cube4d_diffuse[] = {1.0, 0.0, 0.0, 0.0 };
     GLfloat cube4d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4d_shininess[] = { 100.0 };
     GLfloat cube4d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4d_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<  LANTAI  >>>>>>>>>>>>>>>>>>>>
     //lantai belakang
     glPushMatrix();
     glTranslatef(2.8,-1.82,-4.0);
     glScalef(14.19f, 0.4f, 7.0f);
     GLfloat cube1d_diffuse[] = {0.0, 1.0, 1.0, 1.0 };
     GLfloat cube1d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d_shininess[] = { 100.0 };
     GLfloat cube1d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //lantai samping
     glPushMatrix();
     glTranslatef(7.0,-1.82,3.5);
     glScalef(6.0f, 0.4f, 8.0f);
     GLfloat cube2d_diffuse[] = {0.0, 1.0, 1.0, 1.0 };
     GLfloat cube2d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2d_shininess[] = { 100.0 };
     GLfloat cube2d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //lantai tengah
     glPushMatrix();
     glTranslatef(0.7,-1.9,3.0);
     glScalef(10.0f, 0.1f, 9.0f);
     GLfloat cube3d_diffuse[] = {0.0, 1.0, 1.0, 1.0 };
     GLfloat cube3d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3d_shininess[] = { 100.0 };
     GLfloat cube3d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<<<  DINDING BELAKANG  >>>>>>>>>>>>>>>>>>>>
     //dinding belakang
     glPushMatrix();
     glTranslatef(3.0,0.01,-7.3);
     glScalef(14.6f, 3.5f, 0.5f);
     GLfloat cube1d1_diffuse[] = {1.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d1_shininess[] = { 100.0 };
     GLfloat cube1d1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<<<<<  DINDING KIRI  >>>>>>>>>>>>>>>>>>>
     //dinding belakang
     glPushMatrix();
     glTranslatef(10.1,0.5,-5.0);
     glScalef(0.5f, 5.0f, 5.0f);
     GLfloat cube1d2_diffuse[] = {1.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d2_shininess[] = { 100.0 };
     GLfloat cube1d2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //dinding bawah
     glPushMatrix();
     glTranslatef(10.1,-1.0,2.5);
     glScalef(0.5f, 2.0f,10.0f);
     GLfloat cube1d3_diffuse[] = {1.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d3_shininess[] = { 100.0 };
     GLfloat cube1d3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 1
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(10.0,1.2,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d5_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d5_shininess[] = { 100.0 };
     GLfloat cube1d5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d5_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d5_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 2
     glPushMatrix();
     glTranslatef(10.0,0.9,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d6_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d6_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d6_shininess[] = { 100.0 };
     GLfloat cube1d6_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d6_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d6_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d6_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d6_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d6_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d6_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 3
     glPushMatrix();
     glTranslatef(10.0,0.6,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d7_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d7_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d7_shininess[] = { 100.0 };
     GLfloat cube1d7_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d7_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d7_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d7_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d7_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d7_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d7_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 4
     glPushMatrix();
     glTranslatef(10.0,0.3,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d8_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d8_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d8_shininess[] = { 100.0 };
     GLfloat cube1d8_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d8_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d8_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d8_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d8_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d8_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d8_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 5
     glPushMatrix();
     glTranslatef(10.0,1.5,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d9_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d9_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d9_shininess[] = { 100.0 };
     GLfloat cube1d9_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d9_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d9_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d9_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d9_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d9_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d9_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 6
     glPushMatrix();
     glTranslatef(10.0,1.5,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d10_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d10_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d10_shininess[] = { 100.0 };
     GLfloat cube1d10_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d10_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d10_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d10_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d10_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d10_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d10_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 7
     glPushMatrix();
     glTranslatef(10.0,1.8,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d11_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d11_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d11_shininess[] = { 100.0 };
     GLfloat cube1d11_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d11_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d11_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d11_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d11_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d11_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d11_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang melintang 8
     glPushMatrix();
     glTranslatef(10.0,2.1,2.3);
     glScalef(0.1f, 0.1f,9.7f);
     GLfloat cube1d12_diffuse[] = {0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d12_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d12_shininess[] = { 100.0 };
     GLfloat cube1d12_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d12_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d12_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d12_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d12_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d12_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d12_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<<<<<  DINDING KANAN  >>>>>>>>>>>>>>>>>>>
     //dinding kanan
     glPushMatrix();
     glTranslatef(-4.1,0.01,-5.0);
     glScalef(0.5f, 3.7f, 5.0f);
     GLfloat cube1d4_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cube1d4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1d4_shininess[] = { 100.0 };
     GLfloat cube1d4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1d4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1d4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1d4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1d4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1d4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1d4_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<<<<<  KULKAS  >>>>>>>>>>>>>>>>>>>
     //body kulkas
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(-2.8,-0.0,-6.2);
     glScalef(1.7f, 3.4f, 1.5f);
     GLfloat cubekas_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubekas_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekas_shininess[] = { 100.0 };
     GLfloat cubekas_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekas_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekas_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekas_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekas_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekas_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekas_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //pintu bawah
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(-2.8,-0.5,-5.4);
     glScalef(1.6f, 2.0f, 0.1f);
     GLfloat cubekas1_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubekas1_specular[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekas1_shininess[] = { 100.0 };
     GLfloat cubekas1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekas1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekas1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekas1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekas1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekas1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekas1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //pintu atas
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(-2.8,1.1,-5.4);
     glScalef(1.6f, 1.0f, 0.1f);
     GLfloat cubekas2_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubekas2_specular[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekas2_shininess[] = { 100.0 };
     GLfloat cubekas2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekas2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekas2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekas2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekas2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekas2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekas2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

      //pegangan bawah
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(-2.2,-0.3,-5.33);
     glScalef(0.5f, 4.0f, 0.6f);
     GLfloat sphere21lm_diffuse[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere21lm_specular[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere21lm_shininess[] = { 100.0 };
     GLfloat sphere21lm_ambient[]={1.0, 0.0, 0.0, 0.0};
     GLfloat sphere21lm_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, sphere21lm_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, sphere21lm_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, sphere21lm_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, sphere21lm_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, sphere21lm_diffuse);
     glutSolidSphere (0.1, 20,30);
     glPopMatrix();

      //pegangan atas
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(-2.2,1.19,-5.33);
     glScalef(0.5f, 2.0f, 0.6f);
     GLfloat sphere21ln_diffuse[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere21ln_specular[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere21ln_shininess[] = { 100.0 };
     GLfloat sphere21ln_ambient[]={1.0, 0.0, 0.0, 0.0};
     GLfloat sphere21ln_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, sphere21ln_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, sphere21ln_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, sphere21ln_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, sphere21ln_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, sphere21ln_diffuse);
     glutSolidSphere (0.1, 20,30);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<<<<<  CUCI PIRING  >>>>>>>>>>>>>>>>>>>
     //atas
     glPushMatrix();
     glTranslatef(9.0,-0.0,-6.2);
     glScalef(1.7f, 0.3f, 1.5f);
     GLfloat cubepir_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubepir_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubepir_shininess[] = { 100.0 };
     GLfloat cubepir_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubepir_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubepir_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubepir_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubepir_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubepir_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubepir_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //bawah
     glPushMatrix();
     glTranslatef(8.5,-0.9,-6.2);
     glScalef(3.6f, 1.5f, 1.5f);
     GLfloat cubepir1_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubepir1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubepir1_shininess[] = { 100.0 };
     GLfloat cubepir1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubepir1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubepir1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubepir1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubepir1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubepir1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubepir1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //pintu
     glPushMatrix();
     glTranslatef(8.92,-1.0,-5.4);
     glScalef(1.6f, 1.0f, 0.1f);
     GLfloat cubepir2_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubepir2_specular[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubepir2_shininess[] = { 100.0 };
     GLfloat cubepir2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubepir2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubepir2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubepir2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubepir2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubepir2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubepir2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //pintu 2
     glPushMatrix();
     glTranslatef(7.4,-1.0,-5.4);
     glScalef(1.2f, 1.0f, 0.1f);
     GLfloat cubepir3_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubepir3_specular[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubepir3_shininess[] = { 100.0 };
     GLfloat cubepir3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubepir3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubepir3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubepir3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubepir3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubepir3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubepir3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

      //pegangan
     glPushMatrix();
     glTranslatef(7.8,-1.0,-5.33);
     glScalef(0.9f, 0.9f, 0.9f);
     GLfloat sphere1_diffuse[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere1_specular[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere1_shininess[] = { 100.0 };
     GLfloat sphere1_ambient[]={1.0, 0.0, 0.0, 0.0};
     GLfloat sphere1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, sphere1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, sphere1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, sphere1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, sphere1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, sphere1_diffuse);
     glutSolidSphere (0.1, 20,30);
     glPopMatrix();

      //pegangan 2
     glPushMatrix();
     glTranslatef(8.3,-1.0,-5.33);
     glScalef(0.9f, 0.9f, 0.9f);
     GLfloat sphere12_diffuse[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere12_specular[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere12_shininess[] = { 100.0 };
     GLfloat sphere12_ambient[]={1.0, 0.0, 0.0, 0.0};
     GLfloat sphere12_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, sphere12_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, sphere12_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, sphere12_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, sphere12_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, sphere12_diffuse);
     glutSolidSphere (0.1, 20,30);
     glPopMatrix();

//<<<<<<<<<<<<<<<<<<<<<  KERAN  >>>>>>>>>>>>>>>>>>>
     //atas
     glPushMatrix();
     glTranslatef(9.0,0.7,-6.8);
     glScalef(0.1f, 0.1f, 0.7f);
     GLfloat cubek_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubek_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubek_shininess[] = { 100.0 };
     GLfloat cubek_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubek_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubek_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubek_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubek_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubek_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubek_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //depan
     glPushMatrix();
     glTranslatef(9.0,0.65,-6.4);
     glScalef(0.1f, 0.3f, 0.1f);
     GLfloat cubek2_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubek2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubek2_shininess[] = { 100.0 };
     GLfloat cubek2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubek2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubek2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubek2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubek2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubek2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubek2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tekanan
     glPushMatrix();
     glTranslatef(9.0,0.82,-6.5);
     glScalef(0.1f, 0.04f, 0.4f);
     GLfloat cubek3_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubek3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubek3_shininess[] = { 100.0 };
     GLfloat cubek3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubek3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubek3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubek3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubek3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubek3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubek3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();


//<<<<<<<<<<<<<<<<<<<<<  TIANG  >>>>>>>>>>>>>>>>>>>
     //tiang depan kanan
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(-4.0,0.5,7.0);
     glScalef(0.3f, 5.0f, 0.3f);
     GLfloat cubet1_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet1_shininess[] = { 100.0 };
     GLfloat cubet1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 1
     glPushMatrix();
     glTranslatef(-3.5,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp_shininess[] = { 100.0 };
     GLfloat cubetp_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 2
     glPushMatrix();
     glTranslatef(-3.0,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp1_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp1_shininess[] = { 100.0 };
     GLfloat cubetp1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp1_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 3
     glPushMatrix();
     glTranslatef(-2.5,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp2_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp2_shininess[] = { 100.0 };
     GLfloat cubetp2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 4
     glPushMatrix();
     glTranslatef(-2.0,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp3_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp3_shininess[] = { 100.0 };
     GLfloat cubetp3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 5
     glPushMatrix();
     glTranslatef(-1.5,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp4_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp4_shininess[] = { 100.0 };
     GLfloat cubetp4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp4_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 6
     glPushMatrix();
     glTranslatef(-1.0,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp5_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp5_shininess[] = { 100.0 };
     GLfloat cubetp5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp5_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp5_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

      //tiang baris depan 7
     glPushMatrix();
     glTranslatef(-0.5,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp6_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp6_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp6_shininess[] = { 100.0 };
     GLfloat cubetp6_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp6_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp6_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp6_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp6_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp6_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp6_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 8
     glPushMatrix();
     glTranslatef(-0.0,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp7_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp7_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp7_shininess[] = { 100.0 };
     GLfloat cubetp7_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp7_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp7_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp7_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp7_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp7_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp7_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 9
     glPushMatrix();
     glTranslatef(0.5,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp8_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp8_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp8_shininess[] = { 100.0 };
     GLfloat cubetp8_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp8_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp8_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp8_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp8_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp8_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp8_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris depan 10
     glPushMatrix();
     glTranslatef(0.0,0.5,7.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubetp9_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubetp9_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubetp9_shininess[] = { 100.0 };
     GLfloat cubetp9_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubetp9_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubetp9_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubetp9_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubetp9_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubetp9_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubetp9_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang belakang kanan
     glPushMatrix();
     glTranslatef(-4.0,0.5,-7.0);
     glScalef(0.3f, 5.0f, 0.3f);
     GLfloat cubet4_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet4_shininess[] = { 100.0 };
     GLfloat cubet4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet4_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris kanan 1
     glPushMatrix();
     glTranslatef(-4.0,0.5,-1.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubet5_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet5_shininess[] = { 100.0 };
     GLfloat cubet5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet5_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet5_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris kanan 2
     glPushMatrix();
     glTranslatef(-4.0,0.5,-1.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubet6_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet6_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet6_shininess[] = { 100.0 };
     GLfloat cubet6_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet6_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet6_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet6_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet6_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet6_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet6_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris kanan 3
     glPushMatrix();
     glTranslatef(-4.0,0.5,-2.0);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubet7_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet7_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet7_shininess[] = { 100.0 };
     GLfloat cubet7_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet7_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet7_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet7_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet7_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet7_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet7_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang baris kanan 4
     glPushMatrix();
     glTranslatef(-4.0,0.5,-2.5);
     glScalef(0.2f, 5.0f, 0.2f);
     GLfloat cubet8_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet8_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet8_shininess[] = { 100.0 };
     GLfloat cubet8_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet8_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet8_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet8_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet8_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet8_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet8_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang depan tengah
     glPushMatrix();
     glTranslatef(4.3,0.5,7.0);
     glScalef(0.3f, 5.0f, 0.3f);
     GLfloat cubet2_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet2_shininess[] = { 100.0 };
     GLfloat cubet2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet2_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //tiang depan kiri
     glPushMatrix();
     glTranslatef(9.8,0.5,7.0);
     glScalef(0.3f, 5.0f, 0.3f);
     GLfloat cubet3_diffuse[] = {0.1, 0.1, 0.0, 0.1 };
     GLfloat cubet3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubet3_shininess[] = { 100.0 };
     GLfloat cubet3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubet3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubet3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubet3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubet3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubet3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubet3_diffuse);
     glutSolidCube(1.0);
     glPopMatrix();

     //lampu atas meja
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(0.5,2.6,3.3);
     glScalef(0.05f, 1.5f, 0.05f);
     GLfloat cubelamp_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubelamp_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubelamp_shininess[] = { 100.0 };
     GLfloat cubelamp_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubelamp_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubelamp_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubelamp_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubelamp_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubelamp_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubelamp_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //lampu
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(0.5,1.9,3.3);
     glScalef(3.5f, 3.5f, 3.5f);
     GLfloat sphere1l_diffuse[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere1l_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat sphere1l_shininess[] = { 100.0 };
     GLfloat sphere1l_ambient[]={1.0, 0.0, 0.0, 0.0};
     GLfloat sphere1l_lm_ambient[]={0.0,0.0,0.0,0.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, sphere1l_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, sphere1l_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, sphere1l_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, sphere1l_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, sphere1l_diffuse);
     glutSolidSphere (0.1, 20,30);
     glPopMatrix();

     //lampu atas  2
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(7.8,2.6,3.3);
     glScalef(0.05f, 1.5f, 0.05f);
     GLfloat cubelamp2_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubelamp2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubelamp2_shininess[] = { 100.0 };
     GLfloat cubelamp2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubelamp2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubelamp2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubelamp2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubelamp2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubelamp2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubelamp2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //lampu 2
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(7.8,1.9,3.3);
     glScalef(3.5f, 3.5f, 3.5f);
     GLfloat sphere2l_diffuse[] = { 1.0, 0.0, 0.0, 0.0 };
     GLfloat sphere2l_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat sphere2l_shininess[] = { 100.0 };
     GLfloat sphere2l_ambient[]={1.0, 0.0, 0.0, 0.0};
     GLfloat sphere2l_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, sphere2l_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, sphere2l_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, sphere2l_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, sphere2l_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, sphere2l_diffuse);
     glutSolidSphere (0.1, 20,30);
     glPopMatrix();
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<  MEJA>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(0.5,-0.8,3.3);
     glScalef(5.0f, 0.1f, 5.0f);
     GLfloat cube4mj_diffuse[] = { 0.1, 0.0, 0.0, 0.0 };
     GLfloat cube4mj_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4mj_shininess[] = { 100.0 };
     GLfloat cube4mj_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4mj_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4mj_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4mj_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4mj_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4mj_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4mj_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(0.5,-0.9,3.3);
     glScalef(4.5f, 0.4f, 4.5f);
     GLfloat cube4amj_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube4amj_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4amj_shininess[] = { 100.0 };
     GLfloat cube4amj_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4amj_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4amj_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4amj_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4amj_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4amj_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4amj_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glColor3f(1.0,0.0,0.0);
     glTranslatef(1.5,-1.6,2.1);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cube4_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4_shininess[] = { 100.0 };
     GLfloat cube4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.5,-1.6,2.1);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cube4b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube4b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4b_shininess[] = { 100.0 };
     GLfloat cube4b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.5,-1.6,4.5);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cube5_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube5_shininess[] = { 100.0 };
     GLfloat cube5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube5_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube5_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.5,-1.6,4.5);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cube6_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube6_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube6_shininess[] = { 100.0 };
     GLfloat cube6_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube6_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube6_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube6_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube6_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube6_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube6_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi1>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(1.3,-1.3,1);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube1kur_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1kur_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1kur_shininess[] = { 100.0 };
     GLfloat cube1kur_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1kur_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1kur_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1kur_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1kur_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1kur_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1kur_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.3,-0.8,0.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ku_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ku_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ku_shininess[] = { 100.0 };
     GLfloat cube1ku_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ku_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ku_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ku_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ku_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ku_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1,-1.4,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs1_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs1_shininess[] = { 100.0 };
     GLfloat cubekurs1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs1_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.6,-1.4,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs2_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs2_shininess[] = { 100.0 };
     GLfloat cubekurs2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.6,-1.6,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs3_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs3_shininess[] = { 100.0 };
     GLfloat cubekurs3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.0,-1.6,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs4_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs4_shininess[] = { 100.0 };
     GLfloat cubekurs4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs4_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi2>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(-0.5,-1.3,1);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube2kur_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube2kur_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2kur_shininess[] = { 100.0 };
     GLfloat cube2kur_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2kur_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2kur_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2kur_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2kur_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2kur_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2kur_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.5,-0.8,0.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ku2_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ku2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ku2_shininess[] = { 100.0 };
     GLfloat cube1ku2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ku2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ku2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ku2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ku2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ku2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.2,-1.4,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs1b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs1b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs1b_shininess[] = { 100.0 };
     GLfloat cubekurs1b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs1b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs1b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs1b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs1b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs1b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs1b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.8,-1.4,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs2c_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs2c_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs2c_shininess[] = { 100.0 };
     GLfloat cubekurs2c_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs2c_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs2c_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs2c_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs2c_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs2c_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs2c_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.2,-1.6,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs3d_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs3d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs3d_shininess[] = { 100.0 };
     GLfloat cubekurs3d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs3d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs3d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs3d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs3d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs3d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs3d_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.8,-1.6,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs4e_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs4e_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs4e_shininess[] = { 100.0 };
     GLfloat cubekurs4e_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs4e_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs4e_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs4e_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs4e_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs4e_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs4e_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi3>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(-0.5,-1.3,5.5);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube3kur_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube3kur_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3kur_shininess[] = { 100.0 };
     GLfloat cube3kur_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3kur_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube3kur_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube3kur_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube3kur_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube3kur_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube3kur_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.5,-0.8,5.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ku3_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ku3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ku3_shininess[] = { 100.0 };
     GLfloat cube1ku3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ku3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ku3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ku3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ku3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ku3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.2,-1.4,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs13b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs13b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs13b_shininess[] = { 100.0 };
     GLfloat cubekurs13b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs13b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs13b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs13b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs13b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs13b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs13b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.8,-1.4,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs23c_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs23c_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs23c_shininess[] = { 100.0 };
     GLfloat cubekurs23c_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs23c_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs23c_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs23c_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs23c_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs23c_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs23c_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.2,-1.6,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs33d_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs33d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs33d_shininess[] = { 100.0 };
     GLfloat cubekurs33d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs33d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs33d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs33d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs33d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs33d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs33d_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.8,-1.6,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs43e_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs43e_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs43e_shininess[] = { 100.0 };
     GLfloat cubekurs43e_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs43e_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs43e_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs43e_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs43e_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs43e_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs43e_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi4>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(1.3,-1.3,5.5);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube4kur_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube4kur_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4kur_shininess[] = { 100.0 };
     GLfloat cube4kur_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4kur_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4kur_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4kur_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4kur_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4kur_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4kur_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.3,-0.8,5.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ku4_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ku4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ku4_shininess[] = { 100.0 };
     GLfloat cube1ku4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ku4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ku4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ku4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ku4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ku4_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1,-1.4,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs14b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs14b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs14b_shininess[] = { 100.0 };
     GLfloat cubekurs14b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs14b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs14b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs14b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs14b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs14b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs14b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.6,-1.4,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs24c_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs24c_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs24c_shininess[] = { 100.0 };
     GLfloat cubekurs24c_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs24c_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs24c_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs24c_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs24c_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs24c_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs24c_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.6,-1.6,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs34d_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs34d_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs34d_shininess[] = { 100.0 };
     GLfloat cubekurs34d_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs34d_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs34d_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs34d_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs34d_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs34d_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs34d_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1,-1.6,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs44e_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs44e_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs44e_shininess[] = { 100.0 };
     GLfloat cubekurs44e_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs44e_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs44e_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs44e_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs44e_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs44e_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs44e_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();


     //kursi atas 1>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(-0.5,-1.0,-1.7);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cubeat_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubeat_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubeat_shininess[] = { 100.0 };
     GLfloat cubeat_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubeat_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubeat_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubeat_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubeat_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubeat_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubeat_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.5,-0.4,-1.45);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1at_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1at_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1at_shininess[] = { 100.0 };
     GLfloat cube1at_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1at_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1at_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1at_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1at_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1at_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1at_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.8,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube2at_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2at_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2at_shininess[] = { 100.0 };
     GLfloat cube2at_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2at_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2at_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2at_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2at_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2at_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2at_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.2,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube3at_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube3at_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3at_shininess[] = { 100.0 };
     GLfloat cube3at_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3at_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube3at_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube3at_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube3at_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube3at_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube3at_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.2,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube4at_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube4at_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4at_shininess[] = { 100.0 };
     GLfloat cube4at_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4at_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4at_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4at_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4at_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4at_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4at_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-0.8,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube5at_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube5at_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube5at_shininess[] = { 100.0 };
     GLfloat cube5at_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube5at_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube5at_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube5at_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube5at_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube5at_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube5at_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi atas 2>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(-1.8,-1.0,-1.7);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cubeats_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubeats_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubeats_shininess[] = { 100.0 };
     GLfloat cubeats_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubeats_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubeats_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubeats_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubeats_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubeats_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubeats_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-1.8,-0.4,-1.45);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1at2_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1at2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1at2_shininess[] = { 100.0 };
     GLfloat cube1at2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1at2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1at2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1at2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1at2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1at2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1at2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-2.1,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube2at2_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2at2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2at2_shininess[] = { 100.0 };
     GLfloat cube2at2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2at2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2at2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2at2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2at2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2at2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2at2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-1.5,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube3at3_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube3at3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3at3_shininess[] = { 100.0 };
     GLfloat cube3at3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3at3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube3at3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube3at3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube3at3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube3at3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube3at3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-2.1,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube4at4_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube4at4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4at4_shininess[] = { 100.0 };
     GLfloat cube4at4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4at4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4at4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4at4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4at4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4at4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4at4_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-1.5,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube5at5_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube5at5_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube5at5_shininess[] = { 100.0 };
     GLfloat cube5at5_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube5at5_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube5at5_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube5at5_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube5at_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube5at5_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube5at5_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi atas 3>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(-3.0,-1.0,-1.7);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cubeats3_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubeats3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubeats3_shininess[] = { 100.0 };
     GLfloat cubeats3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubeats3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubeats3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubeats3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubeats3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubeats3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubeats3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-3.0,-0.4,-1.45);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1at3_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1at3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1at3_shininess[] = { 100.0 };
     GLfloat cube1at3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1at3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1at3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1at3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1at3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1at3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1at3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-3.3,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube2at3_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2at3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2at3_shininess[] = { 100.0 };
     GLfloat cube2at3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2at3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2at3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2at3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2at3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2at3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2at3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-2.7,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube3at3b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube3at3b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3at3b_shininess[] = { 100.0 };
     GLfloat cube3at3b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3at3b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube3at3b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube3at3b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube3at3b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube3at3b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube3at3b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-3.3,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube4at4b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube4at4b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4at4b_shininess[] = { 100.0 };
     GLfloat cube4at4b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4at4b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4at4b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4at4b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4at4b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4at4b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4at4b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(-2.7,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube5at5bs_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube5at5bs_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube5at5bs_shininess[] = { 100.0 };
     GLfloat cube5at5bs_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube5at5bs_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube5at5bs_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube5at5bs_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube5at5bs_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube5at5bs_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube5at5bs_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursi atas 4>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(0.8,-1.0,-1.7);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cubeata_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubeata_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubeata_shininess[] = { 100.0 };
     GLfloat cubeata_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubeata_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubeata_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubeata_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubeata_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubeata_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubeata_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(0.8,-0.4,-1.45);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ata_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ata_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ata_shininess[] = { 100.0 };
     GLfloat cube1ata_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ata_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ata_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ata_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ata_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ata_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ata_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.1,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube2ata_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2ata_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2ata_shininess[] = { 100.0 };
     GLfloat cube2ata_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2ata_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2ata_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2ata_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2ata_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2ata_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2ata_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(0.5,-1.0,-1.4);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube3ata_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube3ata_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3ata_shininess[] = { 100.0 };
     GLfloat cube3ata_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3ata_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube3ata_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube3ata_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube3ata_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube3ata_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube3ata_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(1.1,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube4ata_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube4ata_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4ata_shininess[] = { 100.0 };
     GLfloat cube4ata_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4ata_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4ata_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4ata_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4ata_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4ata_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4ata_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(0.5,-1.45,-1.98);
     glScalef(0.1f, 1.56f, 0.1f);
     GLfloat cube5ata_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube5ata_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube5ata_shininess[] = { 100.0 };
     GLfloat cube5ata_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube5ata_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube5ata_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube5ata_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube5ata_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube5ata_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube5ata_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //<<<<<<<<<<<<<<<<<<<<<<<<<<<<  MEJA kanan >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();

     glTranslatef(7.8,-0.5,3.3);
     glColor3f(1.0,0.0,0.0);
     glScalef(5.0f, 0.1f, 5.0f);
     GLfloat cubemj_diffuse[] = { 0.1, 0.0, 0.0, 0.0 };
     GLfloat cubemj_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubemj_shininess[] = { 100.0 };
     GLfloat cubemj_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubemj_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4mj_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubemj_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubemj_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubemj_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubemj_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7.8,-0.6,3.3);
     glScalef(4.5f, 0.4f, 4.5f);
     GLfloat cubeamj_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubeamj_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubeamj_shininess[] = { 100.0 };
     GLfloat cubeamj_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubeamj_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubeamj_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubeamj_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubeamj_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubeamj_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubeamj_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.8,-1.2,2.1);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cubem_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubem_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubem_shininess[] = { 100.0 };
     GLfloat cubem_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubem_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubem_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubem_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubem_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubem_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubem_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(6.8,-1.2,2.1);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cubeb_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubeb_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubeb_shininess[] = { 100.0 };
     GLfloat cubeb_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubeb_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubeb_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubeb_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubeb_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubeb_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubeb_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.8,-1.2,4.5);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cube5b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube5b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube5b_shininess[] = { 100.0 };
     GLfloat cube5b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube5b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube5b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube5b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube5b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube5b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube5b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(6.8,-1.2,4.5);
     glScalef(0.2f, 1.9f, 0.2f);
     GLfloat cube6b_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube6b_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube6b_shininess[] = { 100.0 };
     GLfloat cube6b_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube6b_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube6b_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube6b_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube6b_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube6b_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube6b_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursikanan1>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glColor3f(1,1,1);
     glTranslatef(8.3,-1.0,1);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cubekkur_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cubekkur_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekkur_shininess[] = { 100.0 };
     GLfloat cubekkur_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekkur_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekkur_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekkur_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekkur_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekkur_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekkur_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.3,-0.5,0.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1kku_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1kku_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1kku_shininess[] = { 100.0 };
     GLfloat cube1kku_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1kku_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1kku_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1kku_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1kku_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1kku_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8,-1.1,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekkurs1_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekkurs1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekkurs1_shininess[] = { 100.0 };
     GLfloat cubekkurs1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekkurs1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekkurs1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekkurs1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekkurs1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekkurs1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekkurs1_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.6,-1.1,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekkurs2_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekkurs2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekkurs2_shininess[] = { 100.0 };
     GLfloat cubekkurs2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekkurs2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekkurs2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekkurs2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekkurs2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekkurs2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekkurs2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.6,-1.3,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekkurs3_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekkurs3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekkurs3_shininess[] = { 100.0 };
     GLfloat cubekkurs3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekkurs3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekkurs3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekkurs3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekkurs3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekkurs3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekkurs3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.0,-1.3,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekkurs4_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekkurs4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekkurs4_shininess[] = { 100.0 };
     GLfloat cubekkurs4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekkurs4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekkurs4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekkurs4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekkurs4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekkurs4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekkurs4_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     //kursikanan2>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(7.0,-1.0,1);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube2kkur_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube2kkur_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2kkur_shininess[] = { 100.0 };
     GLfloat cube2kkur_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2kkur_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2kkur_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2kkur_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2kkur_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2kkur_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2kkur_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7.0,-0.5,0.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube21kku_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube21kku_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube21kku_shininess[] = { 100.0 };
     GLfloat cube21kku_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube21kku_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube21kku_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube21kku_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube21kku_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube21kku_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube21kku_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(6.7,-1.1,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube2kkurs1_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2kkurs1_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2kkurs1_shininess[] = { 100.0 };
     GLfloat cube2kkurs1_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2kkurs1_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2kkurs1_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2kkurs1_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2kkurs1_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2kkurs1_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2kkurs1_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7.3,-1.1,0.7);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cube2kkurs2_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2kkurs2_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2kkurs2_shininess[] = { 100.0 };
     GLfloat cube2kkurs2_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2kkurs2_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2kkurs2_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2kkurs2_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2kkurs2_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2kkurs2_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2kkurs2_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(6.7,-1.3,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cube2kkurs3_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2kkurs3_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2kkurs3_shininess[] = { 100.0 };
     GLfloat cube2kkurs3_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2kkurs3_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2kkurs3_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2kkurs3_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2kkurs3_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2kkurs3_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2kkurs3_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7.3,-1.3,1.3);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cube2kkurs4_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cube2kkurs4_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube2kkurs4_shininess[] = { 100.0 };
     GLfloat cube2kkurs4_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube2kkurs4_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube2kkurs4_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube2kkurs4_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube2kkurs4_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube2kkurs4_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube2kkurs4_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();


     //kursi3kanan>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(8.3,-1.0,5.5);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube3kurk_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube3kurk_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube3kurk_shininess[] = { 100.0 };
     GLfloat cube3kurk_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube3kurk_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube3kurk_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube3kurk_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube3kurk_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube3kurk_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube3kurk_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.3,-0.5,5.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ku3k_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ku3k_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ku3k_shininess[] = { 100.0 };
     GLfloat cube1ku3k_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ku3k_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku3k_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ku3k_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ku3k_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ku3k_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ku3k_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8,-1.1,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs13bk_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs13bk_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs13bk_shininess[] = { 100.0 };
     GLfloat cubekurs13bk_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs13bk_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs13bk_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs13bk_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs13bk_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs13bk_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs13bk_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.6,-1.1,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekursk23c_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekursk23c_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekursk23c_shininess[] = { 100.0 };
     GLfloat cubekursk23c_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekursk23c_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekursk23c_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekursk23c_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekursk23c_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekursk23c_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekursk23c_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8.6,-1.3,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs33dk_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs33dk_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs33dk_shininess[] = { 100.0 };
     GLfloat cubekurs33dk_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs33dk_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs33dk_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs33dk_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs33dk_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs33dk_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs33dk_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(8,-1.3,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs43ek_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs43ek_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs43ek_shininess[] = { 100.0 };
     GLfloat cubekurs43ek_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs43ek_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs43ek_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs43ek_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs43ek_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs43ek_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs43ek_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();


     //kursi4kanan>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     glPushMatrix();
     glTranslatef(7,-1.0,5.5);
     glScalef(1.4f, 0.1f, 1.4f);
     GLfloat cube4kurk_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube4kurk_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube4kurk_shininess[] = { 100.0 };
     GLfloat cube4kurk_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube4kurk_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube4kurk_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube4kurk_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube4kurk_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube4kurk_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube4kurk_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7,-0.5,5.75);
     glScalef(1.4f, 0.6f, 0.1f);
     GLfloat cube1ku4k_diffuse[] = { 0.0, 0.1, 0.0, 0.0 };
     GLfloat cube1ku4k_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cube1ku4k_shininess[] = { 100.0 };
     GLfloat cube1ku4k_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cube1ku4k_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cube1ku4k_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cube1ku4k_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cube1ku4k_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cube1ku4k_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cube1ku4k_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(6.7,-1.1,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs14bk_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs14bk_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs14bk_shininess[] = { 100.0 };
     GLfloat cubekurs14bk_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs14bk_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs14bk_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs14bk_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs14bk_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs14bk_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs14bk_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7.3,-1.1,5.8);
     glScalef(0.1f, 2.1f, 0.1f);
     GLfloat cubekurs24ck_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs24ck_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs24ck_shininess[] = { 100.0 };
     GLfloat cubekurs24ck_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs24ck_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs24ck_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs24ck_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs24ck_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs24ck_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs24ck_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(6.7,-1.3,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs34dk_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs34dk_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs34dk_shininess[] = { 100.0 };
     GLfloat cubekurs34dk_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs34dk_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs34dk_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs34dk_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs34dk_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs34dk_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs34dk_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();

     glPushMatrix();
     glTranslatef(7.3,-1.3,5.2);
     glScalef(0.1f, 1.0f, 0.1f);
     GLfloat cubekurs44ek_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
     GLfloat cubekurs44ek_specular[] = { 0.0, 0.0, 0.0, 0.0 };
     GLfloat cubekurs44ek_shininess[] = { 100.0 };
     GLfloat cubekurs44ek_ambient[]={1.0,1.0,1.0,1.0};
     GLfloat cubekurs44ek_lm_ambient[]={0.2,0.2,0.2,1.0};
     glLightModelfv(GL_LIGHT_MODEL_AMBIENT, cubekurs44ek_lm_ambient);
     glMaterialfv(GL_FRONT, GL_AMBIENT, cubekurs44ek_ambient);
     glMaterialfv(GL_FRONT, GL_SHININESS, cubekurs44ek_shininess);
     glMaterialfv(GL_FRONT, GL_SPECULAR, cubekurs44ek_specular);
     glMaterialfv(GL_FRONT, GL_DIFFUSE, cubekurs44ek_diffuse);
     glutSolidCube(0.6);
     glPopMatrix();
}
//LAMPU
void lampuJalan()
{
   //Tiang Tegak
	glPushMatrix();
	glColor3f(0.5, 0.5, 0.5);
	glScalef(0.1,2.7,0.05);
	glutSolidCube(9.1f);
	glPopMatrix();

    //Tiang Atas
	glPushMatrix();
	glColor3f(0.5f, 0.5f, 0.5f);
	glTranslatef(0.0,10.3,-2.0);
    glScaled(3.5, 1.0 , 9.5);
    glutSolidCube(0.5f);
	glPopMatrix();

	//Lampu
	glPushMatrix();
	glTranslatef(0.0, 8.7, -3.7);
	glColor3f(1, 1, 1);
	glScalef(3.9,2.8,2.5);
	glutSolidSphere(0.5,70,20);
	glPopMatrix();

}
void Plang ()
    //Tiang kanan
{
    glPushMatrix();
	glColor3f(1,1,1);
    glTranslatef(17.0, 5.0, 36.0);
    glScaled(2.0, 50.0 , 2.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    //Tiang kiri
    glPushMatrix();
	glColor3f(1,1,1);
    glTranslatef(-17.0, 5.0, 36.0);
    glScaled(2.0, 50.0 , 2.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    //Plang
    glPushMatrix();
	glColor3f(1,1,1);
    glTranslatef(0.0, 15.0, 36.0);
    glScaled(65.0, 15.0 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

}
//PAGAR
void pagar()
{
    //Pagar Atas
    glPushMatrix();
	glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    glTranslatef(-26.0f, 4.0f, 0.0f);
    glScaled(900.0, 2.0 , 0.6);
    glutSolidCube(0.5f);
    glPopMatrix();

    //Pagar Bawah
    glPushMatrix();
	glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    glTranslatef(-26.0f, 0.05f, 0.0f);
    glScaled(900.0, 2.0 , 0.6);
    glutSolidCube(0.5f);
    glPopMatrix();

    //Pagar Tegak
    for (int i = -9; i < 9; i++)
    {
    glPushMatrix();
	glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    glTranslatef(i*30, 0.0f, 1.0f);
    glScaled(1.5, 20.0 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    }
}


void pohon()
{
    //Batang Cemara
    glPushMatrix();
	glColor3f(0.8f, 0.4f, 0.1f);
    glScaled(3.10,50,3);
    glutSolidCube(0.5f);
    glPopMatrix();

    //Daun Bawah
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(0.0f, 1.5f, 0.0f);
    glRotatef(230, 1.5, 2, 2);
	glScaled(5,5,10);
	glutSolidCone(1.6,1,20,30);
	glPopMatrix();

    //Daun Tengah
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(0.0f, 3.0f, 0.0f);
    glRotatef(230, 1.5, 2, 2);
	glScaled(5,5,10);
	glutSolidCone(1.3,1,20,30);
	glPopMatrix();

	//Daun Atas
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(0.0f, 4.5f, 0.0f);
    glRotatef(230, 1.5, 2, 2);
	glScaled(5,5,10);
	glutSolidCone(1.0,1,20,30);
	glPopMatrix();

}

unsigned int LoadTextureFromBmpFile(char *filename);

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx-190, viewy+130, viewz+250, 0.0, 10.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrain, 0.3f, 0.9f, 0.0f);
	glPopMatrix();

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainJalan, 0.0f, 0.0f, 0.1f);
	glPopMatrix();

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainAir, 0.0f, 0.2f, 0.5f);
	glPopMatrix();

////////////////////////////////Mulai Objek/////////////////////////////////////

//Marka Jalan
for (int i = -3; i < 4; i++)
        {
            glPushMatrix();
            glTranslatef(i*60, 18.25, 165);
            marka();
            glPopMatrix();
        }
//marka2
for (int i = -3; i < 4; i++)
        {
            glPushMatrix();
            glTranslatef(i*60, 18.25, -165);
            marka();
            glPopMatrix();
}
//marka3
/*for (int i = -3; i < 4; i++)
        {
            glPushMatrix();
            glTranslatef(i*90, 18.25, 185);
            glScaled(10,5,1);
            glRotatef(35, 0, 0, 15);
            marka();
            glPopMatrix();
}*/
//marka4
        {
            glPushMatrix();
            glTranslatef(145, 18.55, 85);
            glScaled(10,5,1);
            marka();
            glPopMatrix();
        }
        //marka4
        {
            glPushMatrix();
            glTranslatef(145, 18.55, 55);
            glScaled(10,5,1);
            marka();
            glPopMatrix();
        }
//marka4
        {
            glPushMatrix();
            glTranslatef(145, 18.55, 25);
            glScaled(10,5,1);
            marka();
            glPopMatrix();
        }
//LAMPU JALAN
    for (int i = -2; i < 2; i++)
        {
            glPushMatrix();
            glTranslatef(i*120, 30, 200);
            lampuJalan();
            glPopMatrix();
        }

//Bangunan Kedai
	glPushMatrix();
    glTranslatef (-50, 45.60,-50);
    glScaled(10,15,10);
   bar();
   glPopMatrix();

//PAGAR
        glPushMatrix();
        glTranslatef(10, 20.30, 192);
        pagar();
        glPopMatrix();


//POHON
for (int i = -4; i < 3; i++)
        {
            glPushMatrix();
            glTranslatef(i*30, 20, 125);
            pohon();
            glPopMatrix();
        }
//Pohon pinggir
    glPushMatrix();
    glTranslatef(185, 20.30, 100);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(185, 20.30, 70);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(185, 20.30, 125);
    pohon();
    glPopMatrix();

//Pohon Depan
    glPushMatrix();
    glTranslatef(150, 20.30, 125);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(110, 20.30, 125);
    pohon();
    glPopMatrix();

//Pohon Belakang
    glPushMatrix();
    glTranslatef(190, 20.30, -110);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(190, 20.30, -80);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(180, 20.30, -125);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(150, 20.30, -125);
    pohon();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(125, 20.30, -125);
    pohon();
    glPopMatrix();

//Plang
    glPushMatrix();
    glTranslatef(80, 20.30, 95);
    Plang();
    glPopMatrix();

	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

}

void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainJalan = loadTerrain("heightmapjalan.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);

	//binding texture

}

static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Bangunan Kedai Sushi");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 0;
}
