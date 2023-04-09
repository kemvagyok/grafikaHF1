//=============================================================================================
// //
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : SAGI BENEDEK
// Neptun : ECSGGY
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
#include "framework.h"

const char* const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0

	void main() {
		gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;		// transform vp from modeling space to normalized device space
	}
)";

const char* const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";


inline float lorentzDot(const vec3& v1, const vec3& v2) { return (v1.x * v2.x + v1.y * v2.y - v1.z * v2.z); }
inline float lengthLorentz(const vec3& v) { return sqrtf(lorentzDot(v, v)); }
inline vec3 normalizeLorentz(const vec3& v) { return v * (1 / lengthLorentz(v)); }
inline vec3 differentTwoPoints(const vec3& v1, const vec3& v2) { return vec3(v1.x- v2.x, v1.y - v2.y, v1.z - v2.z); }

vec2 projection(vec3 point)
{
	/*https://bjlkeng.github.io/posts/hyperbolic-geometry-and-poincare-embeddings/ a 10.egyenletének az 1. része*/
	float x = point.x / (point.z + 1);
	float y = point.y / (point.z + 1);
	return vec2(x, y);
}

vec3 backProjectionPoint(vec2 point)
{
	/*https://bjlkeng.github.io/posts/hyperbolic-geometry-and-poincare-embeddings/ a 10.egyenletének az 2. része*/
	float yi2 = point.x * point.x + point.y * point.y;
	float newZ = (1 + yi2) / (1 - yi2);
	float newX = (2 * point.x) / (1 - yi2);
	float newY = (2 * point.y) / (1 - yi2);
	return vec3(newX, newY, newZ);
}

vec3 orthogonallyDirection(vec3 vector, vec3 point)
{
	vec3 tempVec = cross(vec3(vector.x, vector.y, -vector.z), vec3(point.x, point.y, -point.z));
	return tempVec / lorentzDot(tempVec, tempVec);
}

vec3 goingAround(vec3 v, float radian, vec3 center)
{
	return v * cosf(radian) + orthogonallyDirection(v, center) * sinf(radian);
}

vec3 toFindCurrentVector(vec3 center, vec3 startVector)
{
	float scalar = -lorentzDot(center, vec3(startVector.x,startVector.y,0)) / lorentzDot(center, center);
	vec3 vector = vec3(startVector.x, startVector.y, 0) + scalar * center;
	return normalizeLorentz(vector);
}

void twoPointsBetween(vec3 pointDest,vec3 pointStart,float& distance, vec3& direction)
{

	vec3 different = differentTwoPoints(pointDest, pointStart);
	distance = acoshf(-lorentzDot(pointDest, pointStart));
	direction = different / distance;

}

std::vector<vec3> moving(vec3 point, vec3 velocity, float t)
{
	vec3 newPoint = point * coshf(t) + velocity * sinhf(t);
	vec3 newVelocity = point * sinhf(t) + velocity * coshf(t);
	vec2 backPoint = projection(newPoint);
	newPoint = backProjectionPoint(backPoint);
	return std::vector<vec3>{newPoint, newVelocity};
}

vec3 newCreatingPoint(vec3 pointStart, float distance, vec3 direction)
{
	vec3 pointDest;
	double scalar = coshf(distance);
	return pointStart * scalar + direction * sinhf(distance);
}



GPUProgram gpuProgram; 
unsigned int vao;	   
const int nv = 200;

class HyperboloidCircle
{

	float radius;
	float eyeRadius;
	float blueEyeRadius;
	float mouthRadius;
	float mouthSize = -0.00001f;

	vec3 previousCenter;
	std::vector<vec3> previousCenters;
	vec3 center; 
	vec3 velocity;

	vec3 eyeLeftCenter;
	vec3 eyeRightCenter;

	vec2 circleVelocity;
	vec2 circleCenter;

	

public:
	HyperboloidCircle(vec3 center0, vec3 velocity0, float radius0)
	{
		center = center0;
		velocity = velocity0;
		radius = radius0;
		eyeRadius = radius0 / 4;
		blueEyeRadius = radius0 / 6;
		blueEyeRadius = radius0 / 4;

	}
	HyperboloidCircle(vec2 startCenter, vec2 startVelocity, float radius0)
	{
		center = backProjectionPoint(startCenter);
		previousCenter = center;
		velocity = toFindCurrentVector(center, startVelocity);
		radius = radius0;
		eyeRadius = radius0 / 4;
		blueEyeRadius = radius0 / 6;
		mouthRadius = radius0 / 4;
	}
	HyperboloidCircle(vec2 startCenter, vec2 startVelocity)
	{
		radius = 0.2;
		circleCenter = startCenter;
		circleVelocity = startVelocity;
		vec2 orthoCircleVelocity = vec2(circleCenter.y, -circleVelocity.x);
		center = backProjectionPoint(circleVelocity + circleCenter);
		velocity = toFindCurrentVector(center, orthoCircleVelocity);
		eyeRadius = radius / 4;
		blueEyeRadius = radius / 6;
		mouthRadius = radius / 4;
	}

	vec3 getEyeCenter(int elojel)
	{
		if (elojel == 1) return eyeLeftCenter;
		else return eyeRightCenter;
	}

	void moveWithDirect(float t)
	{
		std::vector<vec3> split = moving(center, velocity, t);
		previousCenter = center;
		previousCenters.push_back(previousCenter);
		center = split[0];
		velocity = split[1];
	}



	void moveOnCircle(float t)
	{

		circleVelocity.x =  radius * cosf(t);
		circleVelocity.y = radius * sinf(t);
		center = backProjectionPoint(circleVelocity + circleCenter);
		velocity = toFindCurrentVector(center,-circleVelocity);
		velocity = goingAround(velocity, -M_PI / 2, center);
		

		previousCenter = center;
		previousCenters.push_back(previousCenter);

	}
	void moveGoingAround(int elojel) {

		velocity = goingAround(velocity,elojel * 50 * ((M_PI * 2) / nv), center);
		velocity = toFindCurrentVector(center, velocity);
	}
	std::vector<vec2> getProjectedBodyPoints()
	{
		std::vector<vec3> points = getPoints();
		std::vector<vec2> projectedpoints;
		for (int i = 0; i < points.size(); i++)
		{
			projectedpoints.push_back(projection(points[i]));
		}
		return projectedpoints;
	}

	std::vector<vec2> getEyePoints(int elojel)
	{
		vec3 tempVelocity = velocity;
		tempVelocity = goingAround(tempVelocity, elojel * 20 * ((M_PI * 2) / nv), center);
		
		tempVelocity = toFindCurrentVector(center, tempVelocity);
		vec3 eyeCenter = newCreatingPoint(center, radius, tempVelocity);
		if (elojel == 1)
		{
			eyeLeftCenter = eyeCenter;
		}
		else
		{
			eyeRightCenter = eyeCenter;
		}
		HyperboloidCircle eye = HyperboloidCircle(eyeCenter, tempVelocity, eyeRadius );
		return eye.getProjectedBodyPoints();
	}

	std::vector<vec2> getBlueEyePoints(int elojel, vec3 antotherBlueEye)
	{
		vec3 tempVelocity;
		float distance;
		vec3 pointStart;


		if (elojel == 1)
		{
			pointStart = eyeLeftCenter;
		}
		else
		{
			pointStart = eyeRightCenter;
		}

		twoPointsBetween(antotherBlueEye, pointStart, distance, tempVelocity);
	
		tempVelocity = normalizeLorentz(tempVelocity) * (radius / 4);
		vec3 eyeBlueCenter = newCreatingPoint(center, radius, tempVelocity);
		HyperboloidCircle blueEye = HyperboloidCircle(pointStart+ tempVelocity, tempVelocity, blueEyeRadius);
		return blueEye.getProjectedBodyPoints();

	}

	std::vector<vec2> getMouthPoints()
	{
		vec3 tempVelocity = velocity;
		tempVelocity = toFindCurrentVector(center, tempVelocity);
		vec3 eyeCenter = newCreatingPoint(center, radius, tempVelocity);
		HyperboloidCircle mouth = HyperboloidCircle(eyeCenter, tempVelocity, mouthRadius);
		mouthRadius += mouthSize;
		if (mouthRadius < 0)
		{
			mouthSize *= -1;
			mouthRadius += mouthSize;
		}
		else if(mouthRadius>radius/4) {
			mouthSize *= -1;
			mouthRadius += mouthSize;
		}
		return mouth.getProjectedBodyPoints();
	}

	vec2 getProjectedCenter()
	{
		return projection(center);
	}


	std::vector<vec2> getProjectedPath(float t)
	{


		std::vector<vec2> projectedPoints;
		for (int i = 0; i < previousCenters.size(); i++)
		{
			projectedPoints.push_back(projection(previousCenters[i]));
		}
		return projectedPoints;
	}


private:
	std::vector<vec3> getPoints()
	{
		std::vector<vec3> newPoints;
		vec3 tempVelocity = velocity;
		for (int i = 0; i < nv; i +=1)
		{
			char eventname[100];
			tempVelocity = goingAround(tempVelocity, i*((M_PI * 2) / nv), center);
			newPoints.push_back(newCreatingPoint(center, radius, tempVelocity));

		}
		return newPoints;
	}
};

class Line
{
	unsigned int vao;
	unsigned int vbo;
	int size;
	vec4 color;
public:
	Line(vec4 color0) {
		color = color0;
		glGenVertexArrays(1, &vao); 
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(
			0,
			2, GL_FLOAT,
			GL_FALSE,
			0, NULL
		);
	}

	void updateGPU(std::vector<vec2> vertices)
	{
		size = vertices.size();
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER,vbo);
		glBufferData(GL_ARRAY_BUFFER, size * sizeof(vec2), &vertices[0], GL_DYNAMIC_DRAW);
	}

	void Draw()
	{

		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, color.x, color.y, color.z);
		glBindVertexArray(vao);
		glDrawArrays(GL_LINE_STRIP, 0, size);
	}
};



class Circle
{
	unsigned int vao;
	unsigned int vbo;
	vec4 color;
public:
	Circle(vec4 color0) {
		color = color0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(
			0,
			2, GL_FLOAT,
			GL_FALSE,
			0, NULL
		);
	}

	void updateGPU(std::vector<vec2> vertices)
	{
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, nv * sizeof(vec2), &vertices[0], GL_DYNAMIC_DRAW);
	}
	void Draw()
	{
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, color.x,color.y,color.z); // 3 floats

		float MVPtransf[4][4] = { 
								1, 0, 0, 0,    // MVP matrix, 								  
								0, 1, 0, 0,    // row-major!
								0, 0, 1, 0,
								0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");	
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, nv);
	}
};


HyperboloidCircle* hyperboloid_Player;
HyperboloidCircle* hyperboloid_NPC;


Circle* poincare;


Circle* poincare_Player;
Circle* poincare_Player_Left_Eye;
Circle* poincare_Player_Left_BlueEye;
Circle* poincare_Player_Right_Eye;
Circle* poincare_Player_Right_BlueEye;
Circle* poincare_Player_Mouth;

Line* poincare_Player_Path;

Circle* poincare_NPC;
Circle* poincare_NPC_Left_Eye;
Circle* poincare_NPC_Left_BlueEye;
Circle* poincare_NPC_Right_Eye;
Circle* poincare_NPC_Right_BlueEye;
Circle* poincare_NPC_Mouth;

Line* poincare_NPC_Path;

void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	hyperboloid_Player = new HyperboloidCircle(vec2(0.2, 0.2), vec2(-0.2, -0.2), 0.2);
	poincare_Player = new Circle(vec4(1, 0, 0, 1));

	poincare_Player_Left_Eye = new Circle(vec4(1, 1, 1, 1));
	poincare_Player_Left_BlueEye = new Circle(vec4(0, 0, 1, 1));

	poincare_Player_Right_Eye = new Circle(vec4(1, 1, 1, 1));
	poincare_Player_Right_BlueEye = new Circle(vec4(0, 0, 1, 1));

	poincare_Player_Mouth = new Circle(vec4(0, 0, 0, 1));

	poincare_Player_Path = new Line(vec4(1, 1, 1, 1));

	poincare_Player_Mouth = new Circle(vec4(0, 0, 0, 1));
	//*********************
	hyperboloid_NPC = new HyperboloidCircle(vec2(-0.1, -0.1), vec2(0.2,0.2));
	poincare_NPC = new Circle(vec4(0, 1, 0, 1));

	poincare_NPC_Left_Eye = new Circle(vec4(1, 1, 1, 1));
	poincare_NPC_Left_BlueEye = new Circle(vec4(0, 0, 1, 1));

	poincare_NPC_Right_Eye = new Circle(vec4(1, 1, 1, 1));
	poincare_NPC_Right_BlueEye = new Circle(vec4(0, 0, 1, 1));

	poincare_NPC_Mouth = new Circle(vec4(0, 0, 0, 1));


	poincare_NPC_Path = new Line(vec4(1, 1, 1, 1));

	std::vector<vec2> poincarePoints;
	for (int i = 0; i <nv; i++)
	{
		float degree = 2 * M_PI*i / nv;
;		poincarePoints.push_back(vec2(cosf(degree),sinf(degree)));
	}
	poincare = new Circle(vec4(0, 0, 0, 1));
	poincare->updateGPU(poincarePoints);


	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}
float secTime= 0;
float previousTime= 0;
void onDisplay() {
	glClearColor(0.5f,0.5f , 0.5f, 0);     
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	poincare->Draw();

	poincare_Player_Path->updateGPU(hyperboloid_Player->getProjectedPath(0.2));
	poincare_Player->updateGPU(hyperboloid_Player->getProjectedBodyPoints());
	poincare_Player_Left_Eye->updateGPU(hyperboloid_Player->getEyePoints(1));
	poincare_Player_Right_Eye->updateGPU(hyperboloid_Player->getEyePoints(-1));
	poincare_Player_Left_BlueEye->updateGPU(hyperboloid_Player->getBlueEyePoints(1, hyperboloid_NPC->getEyeCenter(1)));
	poincare_Player_Right_BlueEye->updateGPU(hyperboloid_Player->getBlueEyePoints(-1, hyperboloid_NPC->getEyeCenter(-1)));
	poincare_Player_Mouth->updateGPU(hyperboloid_Player->getMouthPoints());


	poincare_Player_Path->Draw();
	poincare_Player->Draw();
	poincare_Player_Left_Eye->Draw();
	poincare_Player_Right_Eye->Draw();
	poincare_Player_Left_BlueEye->Draw();
	poincare_Player_Right_BlueEye->Draw();
	poincare_Player_Mouth->Draw();

	poincare_NPC_Path->updateGPU(hyperboloid_NPC->getProjectedPath(0.2));
	poincare_NPC->updateGPU(hyperboloid_NPC->getProjectedBodyPoints());
	poincare_NPC_Left_Eye->updateGPU(hyperboloid_NPC->getEyePoints(1));
	poincare_NPC_Right_Eye->updateGPU(hyperboloid_NPC->getEyePoints(-1));
	poincare_NPC_Left_BlueEye->updateGPU(hyperboloid_NPC->getBlueEyePoints(1, hyperboloid_Player->getEyeCenter(1)));
	poincare_NPC_Right_BlueEye->updateGPU(hyperboloid_NPC->getBlueEyePoints(-1, hyperboloid_Player->getEyeCenter(-1)));
	poincare_NPC_Mouth->updateGPU(hyperboloid_NPC->getMouthPoints());

	poincare_NPC_Path->Draw();
	poincare_NPC->Draw();
	poincare_NPC_Left_Eye->Draw();
	poincare_NPC_Right_Eye->Draw();
	poincare_NPC_Left_BlueEye->Draw();
	poincare_NPC_Right_BlueEye->Draw();
	poincare_NPC_Mouth->Draw();

	glutSwapBuffers();

	}

void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == 'e') hyperboloid_Player->moveWithDirect(0.2);
	if (key == 'f') hyperboloid_Player->moveGoingAround(1);
	if (key == 's') hyperboloid_Player->moveGoingAround(-1);
}

void onKeyboardUp(unsigned char key, int pX, int pY) {
} 

void onMouseMotion(int pX, int pY) {	
	float cX = 2.0f * pX / windowWidth - 1;	
	float cY = 1.0f - 2.0f * pY / windowHeight;
	printf("Mouse moved to (%3.2f, %3.2f)\n", cX, cY);
}

void onMouse(int button, int state, int pX, int pY) { 
	float cX = 2.0f * pX / windowWidth - 1;	
	float cY = 1.0f - 2.0f * pY / windowHeight;
	const char* buttonStat = "";
	switch (state) {
	case GLUT_DOWN: buttonStat = "pressed"; break;
	case GLUT_UP:   buttonStat = "released"; break;
	}

	switch (button) {
	case GLUT_LEFT_BUTTON:   printf("Left button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);   break;
	case GLUT_MIDDLE_BUTTON: printf("Middle button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY); break;
	case GLUT_RIGHT_BUTTON:  printf("Right button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);  break;
	}
}

void onIdle() {
	secTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
	float betweenTime = secTime - previousTime;
	if(betweenTime>0.1f)
	{
		for (int i = 1 ; i <= betweenTime * 10; i++)
		{
			hyperboloid_NPC->moveOnCircle(previousTime+(i*0.1));
		}
	}
	else {
		hyperboloid_NPC->moveOnCircle(secTime);
	}
	previousTime = secTime;
	glutPostRedisplay();
}
