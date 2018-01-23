/*
 * Testing OpenGL pixel transfer - we want to send pixel data to OpenGL shaders as fast as possible.
 * We simply want to send a LUMA buffer (i.e. just a grayscale 8-bit pixels) that is later on used by the shader program.
 * 
 * (C) 2018 Sampsa Riikonen
 * License : MIT
 * 
 */


/* compile & link with:
 * 
 * c++ --std=c++14 -I/usr/include/libdrm upload_pbo.cpp -lX11 -lGLEW -lGLU -lGL
 * 
 * 
 */

/* run with:
 * 
 * ./a.out 1            Just test the glx infrastructure : creates a window
 * 
 * ./a.out 2            Upload textures with PBOs - observe how different texture formats affect speed
 *                      The problem here is that we just want single-channel data (GL_RED), but OpenGL seems to mess that up
 *                      by converting it to GL_RGBA ..?
 * 
 * ./a.out 3            Tries to upload textures with TBOs - no luck
 * 
 */


#include<GL/glew.h>
#include<GL/glx.h>

#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <string>

#include <iostream>
#include <fstream> // https://stackoverflow.com/questions/9816900/infile-incomplete-type-error
#include <iomanip>
#include <iterator>
#include <sstream>

#include <vector>  
#include <algorithm>
#include <sys/time.h>
#include <time.h>
// #include <linux/time.h>
// #include <sys/sysinfo.h>

#include <map>
#include <list>

#include <chrono> 
#include <thread>

#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <mutex>
#include <condition_variable>

using namespace std::chrono_literals;
using std::this_thread::sleep_for;


namespace glx_attr { // https://stackoverflow.com/questions/11623451/static-vs-non-static-variables-in-namespace
  static int singleBufferAttributes[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_RED_SIZE,      1,   // Request a single buffered color buffer
    GLX_GREEN_SIZE,    1,   // with the maximum number of color bits
    GLX_BLUE_SIZE,     1,   // for each component                     
    None
  };
  static int doubleBufferAttributes[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,  True,  // Request a double-buffered color buffer with 
    GLX_RED_SIZE,      1,     // the maximum number of bits per component    
    GLX_GREEN_SIZE,    1, 
    GLX_BLUE_SIZE,     1,
    None
  };
};


class OpenGLContext {
  
public:
  OpenGLContext();
  ~OpenGLContext();
  
protected: // init'd at constructor
  Display*      display_id;
  bool          doublebuffer_flag;
  GLXContext    glc;
  int*          att;
  Window        root_id;
  XVisualInfo*  vi;
  GLXFBConfig*  fbConfigs;
  Colormap      cmap;
  
public:
  void makeCurrent(Window window_id);
  void loadExtensions();
  Window createWindow();
};




OpenGLContext::OpenGLContext() {  
  // GLXFBConfig *fbConfigs;
  int numReturned;
  
  // initial connection to the xserver
  this->display_id = XOpenDisplay(NULL);
  if (this->display_id == NULL) {
    std::cout << "OpenGLThtead: initGLX: cannot connect to X server" << std::endl;
  }
  
  // glx frame buffer configuration [GLXFBConfig * list of GLX frame buffer configuration parameters] => consistent visual [XVisualInfo] parameters for the X-window
  this->root_id =DefaultRootWindow(this->display_id); // get the root window of this display
    
  /* Request a suitable framebuffer configuration - try for a double buffered configuration first */
  this->doublebuffer_flag=true;
  this->fbConfigs = glXChooseFBConfig(this->display_id,DefaultScreen(this->display_id),glx_attr::doubleBufferAttributes,&numReturned);
  // MEMORY LEAK when running with valgrind, see: http://stackoverflow.com/questions/10065849/memory-leak-using-glxcreatecontext
  
  this->att=glx_attr::doubleBufferAttributes;
  // this->fbConfigs = NULL; // force single buffer
  
  if (this->fbConfigs == NULL) {  /* no double buffered configs available */
    this->fbConfigs = glXChooseFBConfig( this->display_id, DefaultScreen(this->display_id),glx_attr::singleBufferAttributes,&numReturned);
    this->doublebuffer_flag=False;
    this->att=glx_attr::singleBufferAttributes;
  }
    
  if (this->fbConfigs == NULL) {
    std::cout << "OpenGLContext: initGLX: WARNING! no GLX framebuffer configuration" << std::endl;
  }

  this->glc=glXCreateNewContext(this->display_id,this->fbConfigs[0],GLX_RGBA_TYPE,NULL,True);
  if (!this->glc) {
    std::cout << "OpenGLContext: initGLX: FATAL! Could not create glx context"<<std::endl; 
    exit(2);
  }
}


OpenGLContext::~OpenGLContext() {
  XFree(this->fbConfigs);
  glXDestroyContext(this->display_id, this->glc);
  XCloseDisplay(this->display_id);
}



void OpenGLContext::makeCurrent(Window window_id) {
  glXMakeCurrent(this->display_id, window_id, this->glc);
}


void OpenGLContext::loadExtensions() {
  if (GLEW_ARB_pixel_buffer_object) {
    std::cout << "OpenGLContext: loadExtensions: PBO extension already loaded" <<std::endl;
    return;
  }
  else {
    std::cout << "OpenGLContext: loadExtensions: Will load PBO extension" <<std::endl;
  }
  
  this->makeCurrent(this->root_id); // a context must be made current before glew works..
  
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (GLEW_OK != err) {
  /* Problem: glewInit failed, something is seriously wrong. */
  std::cout << "OpenGLContext: loadExtensions: ERROR: " << glewGetErrorString(err) <<std::endl;  
  }
  else {
    if (GLEW_ARB_pixel_buffer_object) {
      std::cout << "OpenGLContext: loadExtensions:  PBO extension found! :)"<<std::endl;
    }
    else {
      std::cout << "OpenGLContext: loadExtensions: WARNING: PBO extension not found! :("<<std::endl;
    }
    if (GLEW_ARB_texture_buffer_object) {
      std::cout << "OpenGLContext: loadExtensions:  TBO extension found! :)"<<std::endl;
    }
    else {
      std::cout << "OpenGLContext: loadExtensions: WARNING: TBO extension not found! :("<<std::endl;
    }
  }
}


Window OpenGLContext::createWindow() {
  Window win_id;
  XSetWindowAttributes swa;
  
  // this->vi  =glXChooseVisual(this->display_id, 0, this->att); // "visual parameters" of the X window
  this->vi =glXGetVisualFromFBConfig( this->display_id, this->fbConfigs[0] ); // another way to do it ..
  
  swa.colormap   =XCreateColormap(this->display_id, this->root_id, (this->vi)->visual, AllocNone);
  swa.event_mask =ExposureMask | KeyPressMask;
  
  win_id =XCreateWindow(this->display_id, this->root_id, 0, 0, 600, 600, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
  XMapWindow(this->display_id, win_id);
  XStoreName(this->display_id, win_id, "test window");

  return win_id;
}



void test_1() { // just create a window
  Window w;
  OpenGLContext ctx = OpenGLContext();
  
  ctx.loadExtensions();
  w=ctx.createWindow();
  ctx.makeCurrent(w);
  
  sleep_for(3s);
}


void test_2() {
  Window  win;
  GLuint  pbo_index, tex_index;
  GLubyte *payload;
  GLint   format, internal_format; 
  GLsizei w, h, size;
  int     i;
  
  OpenGLContext ctx = OpenGLContext();
  
  ctx.loadExtensions();
  win=ctx.createWindow();
  ctx.makeCurrent(win);
  
  /* format              : format of the texture .. OpenGL might and will convert this to internal format
   *                       for example, for glTexImage2D : https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
   *                       "GL_RED : each element is a single red component. The GL converts it to floating point and assembles it into an RGBA element by .."
   * 
   * internal_format     : 
   * 
   * 
   */
  
  
  // *** Slow as hell ***  .. this one works with my shader program .. There I use separate LUMA and CHROMA textures
  // format          =GL_RED; 
  // internal_format =GL_RED;
  
  // *** Slow as a snail in sahara ***
  // format          =GL_RED;
  // internal_format =GL_R8;
  
  // *** This is absolutely fast & beautiful ! ***
  // *** .. but, we only want to upload data with single component to the GPU, not RGBA ..!
  // format             =GL_RGBA;
  // internal_format    =GL_RGBA;
  
  // *** Fast, but does not work with my shader program (god knows what happens with these formats..)
  format             =GL_DEPTH_COMPONENT;
  internal_format    =GL_R8;
  
  w               =1920;
  h               =1080;
  size            =w*h;  // size of a LUMA HD frame
  
  // let's reserve a PBO
  glGenBuffers(1, &pbo_index);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_index);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_STREAM_DRAW); // reserve n_payload bytes to index/handle pbo_id
  
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind (not mandatory)
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_index); // rebind (not mandatory)
  
  payload = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind
  
  std::cout << "pbo " << pbo_index << " at " << (long unsigned int)payload << std::endl;
  
  // let's reserve a texture
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &tex_index);
  
  std::cout << "texture " << tex_index << std::endl;
  
  glBindTexture(GL_TEXTURE_2D, tex_index);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, 0); // no upload, just reserve 
  /* https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml : "GL_RED : each element is a single red component. The GL converts it to floating point and assembles it into an RGBA element by attaching 0 for green" 
   * 
   * 
   */
  glBindTexture(GL_TEXTURE_2D, 0); // unbind
  
  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> dt;
  
  for(i=0;i<10;i++) {
    // "copy" data to pbo
    start = std::chrono::system_clock::now();
    memset(payload,0,size);
    end = std::chrono::system_clock::now();
    dt = end-start;
    std::cout << "memory upload took " << dt.count()*1000 << " ms" << std::endl;
  }
  
  std::cout << std::endl;
  
  for(i=0;i<10;i++) {
    start = std::chrono::system_clock::now();
    // copy from pbo to texture
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_index);
    glBindTexture(GL_TEXTURE_2D, tex_index); // this is the texture we will manipulate
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, GL_UNSIGNED_BYTE, 0); // copy from pbo to texture 
    glBindTexture(GL_TEXTURE_2D, 0); // unbind
    glFinish();
    end = std::chrono::system_clock::now();
    dt = end-start;
    std::cout << "pbo => texture took " << dt.count()*1000 << " ms" << std::endl;
  }
}


void test_3() {
  Window  win;
  GLuint  tbo_index, tex_index;
  GLubyte *payload;
  GLint   format, internal_format; 
  GLsizei w, h, size;
  int     i;
  
  OpenGLContext ctx = OpenGLContext();
  
  ctx.loadExtensions();
  win=ctx.createWindow();
  ctx.makeCurrent(win);
  
  w               =1920;
  h               =1080;
  size            =w*h;  // size of a LUMA HD frame
  
  glEnable(GL_TEXTURE_2D);
  
  // let's reserve a TBO
  glGenBuffers(1, &tbo_index); // a buffer
  glBindBuffer(GL_TEXTURE_BUFFER, tbo_index); // .. what is it
  glBufferData(GL_TEXTURE_BUFFER, size, 0, GL_STREAM_DRAW); // .. how much
  std::cout << "tbo " << tbo_index << std::endl;
  glBindBuffer(GL_TEXTURE_BUFFER, 0); // unbind
  
  // generate a texture
  glGenTextures(1, &tex_index);
  std::cout << "texture " << tex_index << std::endl;
  
  // let's try to get dma to the texture buffer
  glBindBuffer(GL_TEXTURE_BUFFER, tbo_index); // bind
  payload = (GLubyte*)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY); // ** TODO: doesn't work
  glUnmapBuffer(GL_TEXTURE_BUFFER); // release pointer to mapping buffer
  glBindBuffer(GL_TEXTURE_BUFFER, 0); // unbind
  
  std::cout << "tbo " << tbo_index << " at " << (long unsigned int)payload << std::endl;
  
  if (!payload) {
    std::cout << "Could not get tbo memory access!" << std::endl;
    exit(2);
  }
  
  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> dt;
  
  for(i=0;i<10;i++) {
    // "copy" data to tbo
    start = std::chrono::system_clock::now();
    memset(payload,0,size);
    end = std::chrono::system_clock::now();
    dt = end-start;
    std::cout << "memory upload took " << dt.count()*1000 << " ms" << std::endl;
  }
  
  std::cout << std::endl;
  
  for(i=0;i<10;i++) {
    start = std::chrono::system_clock::now();
    // copy from pbo to texture
    glBindTexture(GL_TEXTURE_BUFFER, tex_index);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, tbo_index);
    // glUniform1i(u_tbo_tex, 0); // u_tbo_tex would be from the shader program..
    glBindTexture(GL_TEXTURE_2D, 0); // unbind
    glFinish();
    end = std::chrono::system_clock::now();
    dt = end-start;
    std::cout << "pbo => texture took " << dt.count()*1000 << " ms" << std::endl;
  }
}




int main(int argc, char** argcv) {
  if (argc<2) {
    std::cout << argcv[0] << " needs an integer argument " << std::endl;
    exit(2);
  }
  switch (atoi(argcv[1])) { // choose test
    case(1):
      test_1();
      break;
    case(2):
      test_2();
      break;
    case(3):
      test_3();
      break;
    case(4):
      // test_4();
      break;
    case(5):
      // test_5();
      break;
    case(6):
      // test_6();
      break; 
    default:
      std::cout << "No such test "<<argcv[1]<<" for "<<argcv[0]<<std::endl;
  }
}
