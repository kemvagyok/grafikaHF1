//=============================================================================================
// Path animation with derivative calculation provided by the Clifford algebra
//=============================================================================================
#include "framework.h"

//--------------------------
struct Dnum {
	//--------------------------
	float value, derivative;

	Dnum(float f0 = 0, float d0 = 0) { value = f0, derivative = d0; }
	Dnum operator-() { return Dnum(-value, -derivative); }
	float& f() { return value; }
	float& d() { return derivative; }
};

inline Dnum operator+(Dnum l, Dnum r) { return Dnum(l.f() + r.f(), l.d() + r.d()); }
inline Dnum operator-(Dnum l, Dnum r) { return Dnum(l.f() - r.f(), l.d() - r.d()); }
inline Dnum operator*(Dnum l, Dnum r) { return Dnum(l.f() * r.f(), l.f() * r.d() + l.d() * r.f()); }
inline Dnum operator/(Dnum l, Dnum r) { return Dnum(l.f() / r.f(), (l.d() * r.f() - l.f() * r.d()) / r.f() / r.f()); }

// Elementary functions prepared for the chain rule as well
inline Dnum Sin(Dnum g) { return Dnum(sin(g.f()), cos(g.f()) * g.d()); }
inline Dnum Cos(Dnum g) { return Dnum(cos(g.f()), -sin(g.f()) * g.d()); }
inline Dnum Tan(Dnum g) { return Sin(g) / Cos(g); }
inline Dnum Log(Dnum g) { return Dnum(logf(g.f()), 1 / g.f() * g.d()); }
inline Dnum Exp(Dnum g) { return Dnum(expf(g.f()), expf(g.f()) * g.d()); }
inline Dnum Pow(Dnum g, float n) { return Dnum(powf(g.f(), n), n * powf(g.f(), n - 1) * g.d()); }


// vertex shader in GLSL
const char* const vertexSource = R"(
	#version 330
    precision highp float;

    uniform vec2 point, tangent;

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0

	void main() {
		vec2 normal = vec2(-tangent.y, tangent.x);	// normal vector is orthogonal to the tangent
		// rotate + translate
		vec2 p = (vertexPosition.x * tangent + vertexPosition.y * normal + point); 
		gl_Position = vec4(p.x, p.y, 0, 1); 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char* const fragmentSource = R"(
	#version 330
    precision highp float;

	uniform vec4 color;			// uniform color
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

	void main() {
		fragmentColor = color; 
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders

//--------------------------
class Object {
	//--------------------------
	unsigned int vao;	// vertex array object id
	int nPoints;		// number of vertices
	vec4 color;			// color of the object
public:
	Object(std::vector<vec2>& points, vec4 color0) {
		color = color0;
		nPoints = points.size();

		glGenVertexArrays(1, &vao);	// create 1 vertex array object
		glBindVertexArray(vao);		// make it active
		unsigned int vbo;		    // vertex buffer objects
		glGenBuffers(1, &vbo);	    // Generate 1 vertex buffer objects
		glBindBuffer(GL_ARRAY_BUFFER, vbo); // mate this vbo active

		glBufferData(GL_ARRAY_BUFFER,      // copy to the GPU
			points.size() * sizeof(vec2),  // size of data in bytes
			&points[0],		               // address of the data array on the CPU
			GL_STATIC_DRAW);	           // copy to GPU

		glEnableVertexAttribArray(0);
		// Data organization of Attribute Array 0 
		glVertexAttribPointer(0,			// Attribute Array 0
			2, GL_FLOAT,  // components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);     // stride and offset: it is tightly packed
	}

	void Draw(vec2 point, vec2 tangent, int type = GL_LINE_LOOP) {
		gpuProgram.setUniform(point, "point");	// set uniform parameters
		gpuProgram.setUniform(tangent, "tangent");
		gpuProgram.setUniform(color, "color");

		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(type, 0, nPoints);	// draw a single line loop with vertices defined in vao
	}
};

// The virtual world: collection of two objects
Object* vehicle;	// moving object
Object* path;		// the path of the moving object

// Path of the object and the derivative with Clifford algebra
void Path(float tt, Dnum& x, Dnum& y) {
	Dnum t(tt, 1);
	x = Sin(t) * (Sin(t) + 3) * 0.4 / (Tan(Cos(t)) + 2);
	y = (Cos(Sin(t) * 8 + 1) * 1.2 + 0.2) / (Pow(Sin(t) * Sin(t), 3) + 2);
}

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(3);

	// vehicle
	std::vector<vec2> vehiclePoints{ vec2(0.1, 0), vec2(-0.1, -0.1), vec2(0, 0), vec2(-0.1, 0.1) };
	vec4 color = vec4(1, 1, 0, 1);
	vehicle = new Object(vehiclePoints, color);

	// path
	std::vector<vec2> pathPoints;
	for (float t = 0; t < 2.0f * M_PI; t += 0.01f) {
		Dnum x, y;
		Path(t, x, y);
		pathPoints.push_back(vec2(x.value, y.value)); // store function values as path
	}
	color = vec4(0, 0, 1, 1);
	path = new Object(pathPoints, color);

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "fragmentColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.8, 0.8, 0.8, 1);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	// Draw path
	path->Draw(vec2(0, 0), vec2(1, 0));

	// Animate and draw vehicle
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
	float sec = time / 1000.0f;				// convert msec to sec
	// Compute tangent with derivation
	Dnum x, y;
	Path(sec / 2, x, y);
	vec2 position(x.value, y.value);
	vec2 velocity(x.derivative, y.derivative);
	float speed = length(velocity);	// velocity
	vec2 heading = velocity / speed;
	// Draw with transformation
	vehicle->Draw(position, heading, GL_TRIANGLE_FAN);
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {}
// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {}
// Mouse click event
void onMouse(int button, int state, int pX, int pY) {}
// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	glutPostRedisplay();					// redraw the scene
}