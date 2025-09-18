#include <GLFW/glfw3.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>  
#include <ctime>   


/**
 * "Hello World!" in 3D with spinning tubular letters and 
 * reflections (simulated ray effect). OpenGL + CPU only.
 * 
 */

float angle = 0.0f;

// Initial rotation axis
float axisX = 0.0f;
float axisY = 1.0f;
float axisZ = 0.0f;

// Perturbation function
void perturbAxis(float &x, float &y, float &z, float magnitude=0.05f) {
    x += ((rand() % 2000) / 1000.0f - 1.0f) * magnitude;
    y += ((rand() % 2000) / 1000.0f - 1.0f) * magnitude;
    z += ((rand() % 2000) / 1000.0f - 1.0f) * magnitude;
    // Normalize
    float len = sqrt(x*x + y*y + z*z);
    if(len > 0.0001f) { x/=len; y/=len; z/=len; }
}

void renderText3D(const char* text) {
    glPushMatrix();
    glScalef(0.01f, 0.01f, 0.01f);
    for (const char* p = text; *p; p++) {
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    }
    glPopMatrix();
}

int main(int argc, char** argv) {
    srand(time(nullptr));

    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(800, 600, "Hello World 3D Random Axis", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glutInit(&argc, argv);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // grey20

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0/600.0, 0.1, 100.0);

    int frameCount = 0;

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(0.0, 2.0, 6.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

        // Floor stencil
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glBegin(GL_QUADS);
            glVertex3f(-5, -0.01f, -5);
            glVertex3f(-5, -0.01f, 5);
            glVertex3f(5, -0.01f, 5);
            glVertex3f(5, -0.01f, -5);
        glEnd();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        // Perturb rotation axis every frame
        perturbAxis(axisX, axisY, axisZ);

        // Reflection
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glPushMatrix();
        glScalef(1, -1, 1);
        glTranslatef(0, -0.02f, 0);
        glRotatef(angle, axisX, axisY, axisZ);
        glColor3f(0.6f, 0.6f, 0.8f);
        renderText3D("Hello World!");
        glPopMatrix();

        glDisable(GL_STENCIL_TEST);

        // The aectual text
        glPushMatrix();
        glRotatef(angle, axisX, axisY, axisZ);
        glColor3f(1.0f, 1.0f, 0.0f);
        renderText3D("Hello World!");
        glPopMatrix();

        // Floor
        glColor4f(0.2f, 0.2f, 0.3f, 1.0f);
        glBegin(GL_QUADS);
            glVertex3f(-5, -0.01f, -5);
            glVertex3f(-5, -0.01f, 5);
            glVertex3f(5, -0.01f, 5);
            glVertex3f(5, -0.01f, -5);
        glEnd();

        glfwSwapBuffers(window);
        glfwPollEvents();

        angle += 0.5f;
        frameCount++;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
