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

#include <fcntl.h> 
#include <unistd.h>

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


/** A general purpose shader class.  Subclass for, say:
 * 
 * - RGB interpolation
 * - YUV interpolation 
 * - YUV interpolation and Fisheye projection
 * - etc.
 * 
 * 
 */
class Shader {

public:
  /** Default constructor.  Calls Shader::compile and Shader::findVars
   */
  Shader();
  virtual ~Shader(); ///< Default destructor

protected: // functions that return shader programs
  virtual const char* vertex_shader()     =0;
  virtual const char* fragment_shader()   =0;
  
  
public: // declare GLint variable references here with "* SHADER PROGRAM VAR"
  GLint  transform; ///< OpenGL VERTEX SHADER PROGRAM VAR : transformation matrix
  GLint  position;  ///< OpenGL VERTEX SHADER PROGRAM VAR : position vertex array.  Typically "hard-coded" into the shader code with (location=0)
  GLint  texcoord;  ///< OpenGL VERTEX SHADER PROGRAM VAR : texture coordinate array. Typically "hard-coded" into the shader code with (location=1)
  
protected:
  GLuint program;   ///< OpenGL reference to shader program

public:
  void compile();   ///< Compile shader
  void virtual findVars();  ///< Link shader program variable references to the shader program
  void scale(GLfloat fx, GLfloat fy); ///< Set transformation matrix to simple scaling
  void use();       ///< Use this shader program
  void validate();  ///< Validate shader program
  
};



class YUVShader : public Shader {

public:
  YUVShader();
  ~YUVShader();
  
public: // declare GLint variable references here with "* SHADER PROGRAM VAR"
  GLint  texy; ///< OpenGL VERTEX SHADER PROGRAM VAR : Y texture
  GLint  texu; ///< OpenGL VERTEX SHADER PROGRAM VAR : U texture
  GLint  texv; ///< OpenGL VERTEX SHADER PROGRAM VAR : V texture
  
protected: // functions that return shader programs
  const char* vertex_shader();
  const char* fragment_shader();
  
public: 
  void findVars();
  
};


class YUVBlockShader : public Shader {

public:
  YUVBlockShader();
  ~YUVBlockShader();
  
public: // declare GLint variable references here with "* SHADER PROGRAM VAR"
  //GLint  texy; ///< OpenGL VERTEX SHADER PROGRAM VAR : Y texture
  //GLint  texu; ///< OpenGL VERTEX SHADER PROGRAM VAR : U texture
  //GLint  texv; ///< OpenGL VERTEX SHADER PROGRAM VAR : V texture
  GLint texBlock; // now just a single texture..
  
protected: // functions that return shader programs
  const char* vertex_shader();
  const char* fragment_shader();
  
public: 
  void findVars();
  
};



class OpenGLContext {
  
public:
  OpenGLContext();
  ~OpenGLContext();
  
protected: // glx infrastructure : init'd at constructor
  Display*      display_id;
  bool          doublebuffer_flag;
  GLXContext    glc;
  int*          att;
  Window        root_id;
  XVisualInfo*  vi;
  GLXFBConfig*  fbConfigs;
  Colormap      cmap;
  XWindowAttributes x_window_attr;
  
protected: // opengl vaos etc.
  GLuint        VAO;     ///< id of the vertex array object
  GLuint        VBO;     ///< id of the vertex buffer object
  GLuint        EBO;     ///< id of the element buffer object
  std::array<GLfloat,16> transform; ///< data of the transformation matrix
  std::array<GLfloat,20> vertices;  ///< data of the vertex buffer object
  std::array<GLuint, 6>  indices;   ///< data of the element buffer object
  
public:
  void makeCurrent(Window window_id);
  void loadExtensions();
  Window createWindow();
  void reserve(Shader *shader);
  void renderYUVShader(Window window_id, YUVShader* shader, GLuint y_index, GLuint u_index, GLuint v_index);
  void renderYUVBlockShader(Window window_id, YUVBlockShader* shader, GLuint tex_index);
};


// helper functions
uint readbytes(const char* fname, uint8_t*& buffer) {
  uint      size;
  std::ifstream file;
  
  file.open(fname,std::ios::in|std::ios::binary|std::ios::ate);
  size = file.tellg();

  file.seekg(0,std::ios::beg);
  file.read((char*)buffer,size);
  file.close();
  
  printf("read %i bytes\n",size);
  
  return size;
}


void getPBO(GLuint& index, GLsizei size, GLubyte*& payload) { // modify pointer in-place
  glGenBuffers(1, &index);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, index);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_STREAM_DRAW); // reserve n_payload bytes to index/handle pbo_id
  
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind (not mandatory)
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, index); // rebind (not mandatory)
  
  payload = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  
  std::cout << "getPBO : " << index << " " << (unsigned long)payload << std::endl;
  
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer ** MANDATORY **
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind ** MANDATORY **
}



Shader::Shader() {
  /*
  compile(); // woops.. at constructor time, overwritten virtual methods are NOT called
  use();
  findVars();
  */
}

Shader::~Shader() {
  glDeleteProgram(this->program);
}


void Shader::compile() {
  GLuint id_vertex_shader, id_fragment_shader;
  const char *source;
  int length, cc;
  GLint success;
  GLchar infoLog[512];
  
  std::cout << "Shader: compile: " <<std::endl;
  std::cout << "Shader: compile: vertex program=" << std::endl << vertex_shader() << std::endl;
  std::cout << "Shader: compile: fragment program=" << std::endl << fragment_shader() << std::endl;
  
  // create and compiler vertex shader
  source=vertex_shader();
  id_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  length = std::strlen(source);
  glShaderSource(id_vertex_shader, 1, &source, &length); 
  glCompileShader(id_vertex_shader);
  glGetShaderiv(id_vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(id_vertex_shader, 512, NULL, infoLog);
    std::cout << "Shader: compile: vertex shader program (len="<<length<<") COMPILATION FAILED!" << std::endl << infoLog << std::endl;
  }

  // create and compiler fragment shader
  source=fragment_shader();
  id_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  length = std::strlen(source);
  glShaderSource(id_fragment_shader, 1, &source, &length);   
  glCompileShader(id_fragment_shader);
  glGetShaderiv(id_fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(id_fragment_shader, 512, NULL, infoLog);
    std::cout << "Shader: compile: fragment shader program (len="<<length<<") COMPILATION FAILED!" << std::endl << infoLog << std::endl;
  }

  // Shader Program
  this->program = glCreateProgram();
  std::cout << "Shader: compile: program index=" << this->program << "\n";
  
  glAttachShader(this->program, id_vertex_shader);
  glAttachShader(this->program, id_fragment_shader);
  glLinkProgram(this->program);
  // Print linking errors if any
  glGetProgramiv(this->program, GL_LINK_STATUS, &success);
  if (!success)
  {
    glGetProgramInfoLog(this->program, 512, NULL, infoLog);
    std::cout << "Shader: compile: fragment shader LINKING FAILED!" << std::endl << infoLog << std::endl;
  }
  // Delete the shaders as they're linked into our program now and no longer necessery
  glDeleteShader(id_vertex_shader);
  glDeleteShader(id_fragment_shader);
}


void Shader::findVars() {
  position=0; // this is hard-coded into the shader code (see "location=0")
  texcoord=1; // this is hard-coded into the shader code (see "location=1")
  
  transform=glGetUniformLocation(program,"transform");
  std::cout << "Shader: findVars: Location of the transform matrix: " << transform << std::endl;
}


void Shader::scale(GLfloat fx, GLfloat fy) {
  GLfloat mat[4][4] = {
    {fx,               0.0f,             0.0f,   0.0f}, 
    {0.0f,             fy,               0.0f,   0.0f},
    {0.0f,             0.0f,             1.0f,   0.0f},
    {0.0f,             0.0f,             0.0f,   1.0f}
  };
  glUniformMatrix4fv(transform, 1, GL_FALSE, mat[0]);
}


void Shader::use() {
  std::cout << "Shader: use: using program index=" << this->program << std::endl;
  glUseProgram(this->program);
}


void Shader::validate() {
  GLint params, maxLength;
  //The maxLength includes the NULL character
  // std::vector<GLchar> infoLog(maxLength);
  
  std::cout << std::endl << "Shader: validating program index=" << program << std::endl;
  std::cout              << "Shader: is program              =" << bool(glIsProgram(program)) << std::endl;
  glValidateProgram(program);
  glGetProgramiv(program,GL_VALIDATE_STATUS,&params);
  std::cout              << "Shader: validate status         =" << params << std::endl;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);  
  char infoLog[maxLength];

  glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
  std::cout              << "Shader: infoLog length          =" << maxLength << std::endl;
  std::cout              << "Shader: infoLog                 =" << std::string(infoLog) << std::endl;
  std::cout << std::endl;
  
}


YUVShader::YUVShader() : Shader() {
  compile();
  use();
  findVars();
}

YUVShader::~YUVShader() {
}


void YUVShader::findVars() {
  position=0; // this is hard-coded into the shader code (see "location=0")
  texcoord=1; // this is hard-coded into the shader code (see "location=1")
  
  std::cout << "YUVShader: findVars: Location of position: " << position << std::endl;
  std::cout << "YUVShader: findVars: Location of texcoord: " << texcoord << std::endl;
  
  transform=glGetUniformLocation(program,"transform");
  std::cout << "YUVShader: findVars: Location of the transform matrix: " << transform << std::endl;
  
  texy=glGetUniformLocation(program,"texy");
  std::cout << "YUVShader: findVars: Location of texy: " << texy << std::endl;
  
  texu=glGetUniformLocation(program,"texu");
  std::cout << "YUVShader: findVars: Location of texu: " << texu << std::endl;
  
  texv=glGetUniformLocation(program,"texv");
  std::cout << "YUVShader: findVars: Location of texv: " << texv << std::endl;
}



/*** YUV Shader Program ***/

const char* YUVShader::vertex_shader () { return 
// shader vertex source code
// We swap the y-axis by substracing our coordinates from 1.
// This is done because most images have the top y-axis
// inversed with OpenGL's top y-axis.
// TexCoord = texcoord;
"#version 300 es\n"
"precision mediump float;\n"
// "in vec2 scaling;\n"
"uniform mat4 transform;\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 texcoord;\n"
"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
// "  gl_Position = vec4(position, 1.0f) * vec4(scaling,1.0f,1.0f);\n"
"  gl_Position = transform * vec4(position, 1.0f);\n"
"  TexCoord = vec2(texcoord.x, 1.0 - texcoord.y);\n"
"}\n";
}

const char* YUVShader::fragment_shader  () { return
"#version 300 es\n"
"precision mediump float;\n"
"in vec3 ourColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D texy; // Y \n"
"uniform sampler2D texu; // U \n"
"uniform sampler2D texv; // V \n"
"out vec4 colour;\n"
" // \n"
"vec3 yuv2rgb(in vec3 yuv) \n"
"{ \n"
"    // YUV offset  \n"
"    // const vec3 offset = vec3(-0.0625, -0.5, -0.5); \n"
"    const vec3 offset = vec3(-0.0625, -0.5, -0.5); \n"  
"    // RGB coefficients \n"
"    const vec3 Rcoeff = vec3( 1.164, 0.000,  1.596); \n"
"    const vec3 Gcoeff = vec3( 1.164, -0.391, -0.813); \n"
"    const vec3 Bcoeff = vec3( 1.164, 2.018,  0.000); \n"  
"    vec3 rgb; \n"
"    yuv = clamp(yuv, 0.0, 1.0); \n"
"    yuv += offset; \n"
"    rgb.r = dot(yuv, Rcoeff);  \n"
"    rgb.g = dot(yuv, Gcoeff); \n"  
"    rgb.b = dot(yuv, Bcoeff); \n"  
"    return rgb; \n"
"} \n"
" // \n"
"vec3 get_yuv_from_texture(in vec2 tcoord) \n"
"{ \n"
"    vec3 yuv; \n"
"    yuv.x = texture(texy, tcoord).r; \n"
"    // Get the U and V values \n"
"    yuv.y = texture(texu, tcoord).r; \n"
"    yuv.z = texture(texv, tcoord).r; \n"
"    return yuv; \n"
"} \n"
" // \n"
"vec4 mytexture2D(in vec2 tcoord) \n"
"{ \n"
"    vec3 rgb, yuv; \n"
"    yuv = get_yuv_from_texture(tcoord); \n"
"    // Do the color transform \n"
"    rgb = yuv2rgb(yuv); \n"
"    return vec4(rgb, 1.0); \n"
"} \n"
" // \n"
"void main()\n"
"{\n"
" //      color = texture(ourTexture1, TexCoord); \n"
"   colour = mytexture2D(TexCoord); \n"
"}\n";
}



YUVBlockShader::YUVBlockShader() : Shader() {
  compile();
  use();
  findVars();
}

YUVBlockShader::~YUVBlockShader() {
}


void YUVBlockShader::findVars() {
  position=0; // this is hard-coded into the shader code (see "location=0")
  texcoord=1; // this is hard-coded into the shader code (see "location=1")
  
  std::cout << "YUVBlockShader: findVars: Location of position: " << position << std::endl;
  std::cout << "YUVBlockShader: findVars: Location of texcoord: " << texcoord << std::endl;
  
  transform=glGetUniformLocation(program,"transform");
  std::cout << "YUVBlockShader: findVars: Location of the transform matrix: " << transform << std::endl;
  
  texBlock=glGetUniformLocation(program,"texBlock");
  std::cout << "YUVBlockShader: findVars: Location of texBlock: " << texBlock << std::endl;
  
  /*
  texy=glGetUniformLocation(program,"texy");
  std::cout << "YUVBlockShader: findVars: Location of texy: " << texy << std::endl;
  
  texu=glGetUniformLocation(program,"texu");
  std::cout << "YUVBlockShader: findVars: Location of texu: " << texu << std::endl;
  
  texv=glGetUniformLocation(program,"texv");
  std::cout << "YUVBlockShader: findVars: Location of texv: " << texv << std::endl;
  */
}



/*** YUV Shader Program ***/

const char* YUVBlockShader::vertex_shader () { return 
// shader vertex source code
// We swap the y-axis by substracing our coordinates from 1.
// This is done because most images have the top y-axis
// inversed with OpenGL's top y-axis.
// TexCoord = texcoord;
"#version 300 es\n"
"precision mediump float;\n"
// "in vec2 scaling;\n"
"uniform mat4 transform;\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 texcoord;\n"
"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
// "  gl_Position = vec4(position, 1.0f) * vec4(scaling,1.0f,1.0f);\n"
"  gl_Position = transform * vec4(position, 1.0f);\n"
"  TexCoord = vec2(texcoord.x, 1.0 - texcoord.y);\n"
"}\n";
}

const char* YUVBlockShader::fragment_shader  () { return
"#version 300 es\n"
"precision mediump float;\n"
"in vec3 ourColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D texBlock; \n" // the bgr texture
"out vec4 colour;\n"
" // \n"
"vec3 yuv2rgb(in vec3 yuv) \n"
"{ \n"
"    // YUV offset  \n"
"    // const vec3 offset = vec3(-0.0625, -0.5, -0.5); \n"
"    const vec3 offset = vec3(-0.0625, -0.5, -0.5); \n"  
"    // RGB coefficients \n"
"    const vec3 Rcoeff = vec3( 1.164, 0.000,  1.596); \n"
"    const vec3 Gcoeff = vec3( 1.164, -0.391, -0.813); \n"
"    const vec3 Bcoeff = vec3( 1.164, 2.018,  0.000); \n"  
"    vec3 rgb; \n"
"    yuv = clamp(yuv, 0.0, 1.0); \n"
"    yuv += offset; \n"
"    rgb.r = dot(yuv, Rcoeff);  \n"
"    rgb.g = dot(yuv, Gcoeff); \n"  
"    rgb.b = dot(yuv, Bcoeff); \n"  
"    return rgb; \n"
"} \n"
" // \n"
"vec3 get_yuv_from_texture(in vec2 tcoord) \n"
"{ \n"
"    vec3 yuv; \n"
"    yuv.x = texture(texBlock, tcoord).b; \n" // yuv is carried in bgr
"    // Get the U and V values \n"
"    yuv.y = texture(texBlock, tcoord).g; \n"
"    yuv.z = texture(texBlock, tcoord).r; \n"
"    return yuv; \n"
"} \n"
" // \n"
"vec4 mytexture2D(in vec2 tcoord) \n"
"{ \n"
"    vec3 rgb, yuv; \n"
"    yuv = get_yuv_from_texture(tcoord); \n"
"    // Do the color transform \n"
"    rgb = yuv2rgb(yuv); \n"
"    return vec4(rgb, 1.0); \n"
"} \n"
" // \n"
"void main()\n"
"{\n"
"   // colour = texture(texBlock, TexCoord); \n"
"   colour = mytexture2D(TexCoord); \n"
"}\n";
}



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


void OpenGLContext::reserve(Shader *shader) {
  unsigned int transform_size, vertices_size, indices_size;
  
  transform =std::array<GLfloat,16>{
    1.0f,             0.0f,             0.0f,   0.0f, 
    0.0f,             1.0f,             0.0f,   0.0f,
    0.0f,             0.0f,             1.0f,   0.0f,
    0.0f,             0.0f,             0.0f,   1.0f
  };
  transform_size=sizeof(GLfloat)*transform.size();
  
  vertices =std::array<GLfloat,20>{
    /* Positions          Texture Coords
       Shader class references:
       "position"        "texcoord"
    */
    1.0f,  1.0f, 0.0f,   1.0f, 1.0f, // Top Right
    1.0f, -1.0f, 0.0f,   1.0f, 0.0f, // Bottom Right
   -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, // Bottom Left
   -1.0f,  1.0f, 0.0f,   0.0f, 1.0f  // Top Left 
  };
  vertices_size=sizeof(GLfloat)*vertices.size();
  
  indices =std::array<GLuint,6>{  // Note that we start from 0!
    0, 1, 3, // First Triangle
    1, 2, 3  // Second Triangle
  };
  indices_size=sizeof(GLuint)*indices.size();
  
  // std::cout << "SIZEOF: " << sizeof(vertices) << " " << vertices_size << std::endl; // eh.. its the same
  
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
  
  std::cout << "RenderContext: activate: VAO, VBO, EBO " << VAO << " " << VBO << " " << EBO << std::endl;
  std::cout << "RenderContext: activate: position, texcoord " << shader->position << " " << shader->texcoord << " " << std::endl;
  
  glBindVertexArray(VAO); // VAO works as a "mini program" .. we do all the steps below, when binding the VAO
  
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
  glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices.data(), GL_STATIC_DRAW);
  
  // Position attribute
  glVertexAttribPointer(shader->position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0); // this refers to the "Positions" part of vertices
  // 0: shader prog ref, 3: three elements per vertex
  glEnableVertexAttribArray(shader->position); // this refers to (location=0) in the shader program
  // TexCoord attribute
  glVertexAttribPointer(shader->texcoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat))); // this refers to "Texture Coords" part of vertices
  // 1: shader prog ref, 2: two elements per vertex
  glEnableVertexAttribArray(shader->texcoord); // this refers to (location=1) in the shader program
  
  glBindVertexArray(0); // Unbind VAO
}


void OpenGLContext::renderYUVShader(Window window_id, YUVShader* shader, GLuint y_index, GLuint u_index, GLuint v_index) {  
  // glFlush();
  // glFinish();
    
  if (!glXMakeCurrent(display_id, window_id, glc)) { // choose this x window for manipulation
    std::cout << "RenderGroup: render: WARNING! could not draw"<<std::endl;
  }
  
  XGetWindowAttributes(display_id, window_id, &(x_window_attr));
  XWindowAttributes& wa=x_window_attr; // shorthand
  GLfloat r, dx, dy;
   
  std::cout << "RenderGroup: render: window w, h " <<x_window_attr.width<<" "<<x_window_attr.height<<std::endl;
  
  glViewport(0, 0, x_window_attr.width, x_window_attr.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // clear the screen and the depth buffer
  
  shader->use(); // use the shader
    
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, y_index);
  glUniform1i(shader->texy, 0); // pass variable to shader
  
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, u_index);
  glUniform1i(shader->texu, 1); // pass variable to shader
  
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, v_index);
  glUniform1i(shader->texv, 2); // pass variable to shader
  
  // calculate dimensions
  // (screeny/screenx) / (iy/ix)  =  screeny*ix / screenx*iy
  r=float(wa.height*(1920)) / float(wa.width*(1080));
  if (r<1.){ // screen wider than image
    dy=1;
    dx=r;
  }
  else if (r>1.) { // screen taller than image
    dx=1;
    dy=1/r;
  }
  else {
    dx=1;
    dy=1;
  }
  
  std::cout << "RenderContext: bindVars: dx, dy = " << dx <<" "<<dy<<" "<< std::endl;
  
  // /* // test..
  transform[0]=dx;
  transform[5]=dy;  
  // */
  /*
  transform= {
    {dx                0.0f,             0.0f,   0.0f}, 
    {0.0f,             dy                0.0f,   0.0f},
    {0.0f,             0.0f,             1.0f,   0.0f},
    {0.0f,             0.0f,             0.0f,   1.0f}
  };
  */
  glUniformMatrix4fv(shader->transform, 1, GL_FALSE, transform.data());
  
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  
  // glFinish();
  
  if (doublebuffer_flag) {
    std::cout << "RenderGroup: render: swapping buffers "<<std::endl;
    glXSwapBuffers(display_id, window_id);
  }
  
  // glFlush();
  // glFinish();
}


void OpenGLContext::renderYUVBlockShader(Window window_id, YUVBlockShader* shader, GLuint tex_index) {  
  // glFlush();
  // glFinish();
    
  if (!glXMakeCurrent(display_id, window_id, glc)) { // choose this x window for manipulation
    std::cout << "RenderGroup: render: WARNING! could not draw"<<std::endl;
  }
  
  XGetWindowAttributes(display_id, window_id, &(x_window_attr));
  XWindowAttributes& wa=x_window_attr; // shorthand
  GLfloat r, dx, dy;
   
  std::cout << "RenderGroup: render: window w, h " <<x_window_attr.width<<" "<<x_window_attr.height<<std::endl;
  
  glViewport(0, 0, x_window_attr.width, x_window_attr.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // clear the screen and the depth buffer
  
  shader->use(); // use the shader
    
  std::cout << "passing tex_index, texBlock : " << tex_index << " " << shader->texBlock << std::endl;
  
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_index);
  glUniform1i(shader->texBlock, 0); // pass variable to shader
  
  // calculate dimensions
  // (screeny/screenx) / (iy/ix)  =  screeny*ix / screenx*iy
  r=float(wa.height*(1920)) / float(wa.width*(1080));
  if (r<1.){ // screen wider than image
    dy=1;
    dx=r;
  }
  else if (r>1.) { // screen taller than image
    dx=1;
    dy=1/r;
  }
  else {
    dx=1;
    dy=1;
  }
  
  std::cout << "RenderContext: bindVars: dx, dy = " << dx <<" "<<dy<<" "<< std::endl;
  
  // /* // test..
  transform[0]=dx;
  transform[5]=dy;  
  // */
  /*
  transform= {
    {dx                0.0f,             0.0f,   0.0f}, 
    {0.0f,             dy                0.0f,   0.0f},
    {0.0f,             0.0f,             1.0f,   0.0f},
    {0.0f,             0.0f,             0.0f,   1.0f}
  };
  */
  glUniformMatrix4fv(shader->transform, 1, GL_FALSE, transform.data());
  
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  
  // glFinish();
  
  if (doublebuffer_flag) {
    std::cout << "RenderGroup: render: swapping buffers "<<std::endl;
    glXSwapBuffers(display_id, window_id);
  }
  
  // glFlush();
  // glFinish();
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
  
  // see allowed format/internal format here:
  // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
  
  // *** Slow as hell ***  .. this one works with my shader program .. There I use separate LUMA and CHROMA textures
  // format          =GL_RED; 
  // internal_format =GL_RED;
  
  // *** Slow as a snail in sahara ***
  // format          =GL_RED;
  // internal_format =GL_R8;
  
  // *** This is absolutely fast & beautiful ! ***
  // *** .. but, we only want to upload data with single component to the GPU, not RGBA ..!
  format             =GL_RGBA; // 0.008971 ms
  // internal_format    =GL_RGBA;
  internal_format    =GL_RGBA8; // we must use sized formats since 3.2+ ?
  
  // format             =GL_BGRA; // 0.003769 ms .. but internal format can't be GL_BGRA doesn't make sense
  // internal_format    =GL_BGRA;
  
  // how to transfer this to the shader program .. ?
  // * create a single GL_BGRA "texture" .. eh, where the data has been dumped somehow..
  // * say, let's put: Y=>B, U=>G, V=>R, A=0
  // * .. shader program must do some coreography to pull this one off
  
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
  
  // generate a texture
  glGenTextures(1, &tex_index);
  std::cout << "texture " << tex_index << std::endl;
  
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, tbo_index);
  std::cout << "tbo " << tbo_index << std::endl;
  glBindBuffer(GL_TEXTURE_BUFFER, 0); // unbind
  
  
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


void test_4() {
  Window  win;
  GLuint  y_pbo, u_pbo, v_pbo;
  GLuint  y_tex, u_tex, v_tex;
  GLubyte *y_payload, *u_payload, *v_payload;
  GLubyte *image, *y_image, *u_image, *v_image;
  GLint   format, internal_format; 
  GLsizei w, h, size, yuvsize;
  int     i;
  
  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> dt;
  
  format          =GL_RED; 
  internal_format =GL_RED;
  
  OpenGLContext ctx = OpenGLContext();
  
  ctx.loadExtensions();
  win=ctx.createWindow();
  ctx.makeCurrent(win);
  
  YUVShader *shader = new YUVShader();
  
  ctx.reserve(shader); // reserve stuff .. and communicate with the shader about the whereabouts of that stuff
  
  // w               =1920;
  // h               =1080;
  
  w               =1280;
  h               =720;
  
  size            =w*h;  // single plane size
  yuvsize         =(3*size)/2; // all planes in yuv
  
  image   = new GLubyte[yuvsize];
  y_image = new GLubyte[size];
  u_image = new GLubyte[size/4];
  v_image = new GLubyte[size/4];
  
  // rgb : w*h*3
  // yuv planes : 1 + 2*(1/4) = 1+1/2 = 3/2 = (3/2) * w*h 
  
  // load the image
  std::cout << "read " << readbytes("1.yuv",image) <<" bytes" << std::endl;
  std::cout << "should be " << yuvsize << " bytes" << std::endl;
  
  memcpy(y_image, image,              size  );
  memcpy(u_image, &image[size],       size/4);
  memcpy(v_image, &image[(5*size)/4], size/4); // 4/4 + 1/4 = 5/4
  // return;
  
  // let's reserve PBOs
  getPBO(y_pbo,size,   y_payload);
  getPBO(u_pbo,size/4, u_payload);
  getPBO(v_pbo,size/4, v_payload);
  
  // let's create yuv textures
  glEnable(GL_TEXTURE_2D);
  
  glGenTextures(1, &y_tex);
  glBindTexture(GL_TEXTURE_2D, y_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, 0); 
  
  glGenTextures(1, &u_tex);
  glBindTexture(GL_TEXTURE_2D, u_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w/2, h/2, 0, format, GL_UNSIGNED_BYTE, 0); 
  
  glGenTextures(1, &v_tex);
  glBindTexture(GL_TEXTURE_2D, v_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w/2, h/2, 0, format, GL_UNSIGNED_BYTE, 0); 
  
  glBindTexture(GL_TEXTURE_2D, 0); // unbind
  
  // ok
  
  // upload
  //memcpy(y_payload, image,            size  );
  //memcpy(u_payload, image+size,       size/4);
  //memcpy(v_payload, image+(5*size)/4, size/4); // 4/4 + 1/4 = 5/4
  
  memcpy(y_payload, y_image,  size  );
  memcpy(u_payload, u_image,  size/4);
  memcpy(v_payload, v_image,  size/4); // 4/4 + 1/4 = 5/4
  
  sleep_for(1s); // give it time to upload
  
  for(i=0;i<10;i++) {
    start = std::chrono::system_clock::now();
  
    // y
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, y_pbo);
    glBindTexture(GL_TEXTURE_2D, y_tex); // this is the texture we will manipulate
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, GL_UNSIGNED_BYTE, 0); // copy from pbo to texture 
    glBindTexture(GL_TEXTURE_2D, 0); 
    
    // u
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, u_pbo);
    glBindTexture(GL_TEXTURE_2D, u_tex); // this is the texture we will manipulate
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w/2, h/2, format, GL_UNSIGNED_BYTE, 0); // copy from pbo to texture 
    glBindTexture(GL_TEXTURE_2D, 0);  
    
    // v
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, v_pbo);
    glBindTexture(GL_TEXTURE_2D, v_tex); // this is the texture we will manipulate
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w/2, h/2, format, GL_UNSIGNED_BYTE, 0); // copy from pbo to texture 
    glBindTexture(GL_TEXTURE_2D, 0); 
    
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind // important!
    glBindTexture(GL_TEXTURE_2D, 0); // unbind
    
    glFlush();
    glFinish();
  
    end = std::chrono::system_clock::now();
    dt = end-start;
    std::cout << "pbo => tex took " << dt.count()*1000 << " ms" << std::endl;
  }
    
  ctx.renderYUVShader(win, shader, y_tex, u_tex, v_tex);
  
  sleep_for(5s);
  
}


void test_5() {
  Window  win;
  // GLuint  y_pbo, u_pbo, v_pbo;
  // GLuint  y_tex, u_tex, v_tex;
  GLuint  pbo, tex;
  // GLubyte *y_payload, *u_payload, *v_payload;
  GLubyte    *payload, *dummypayload;
  GLubyte *image, *y_image, *u_image, *v_image;
  GLint   format, internal_format; 
  GLsizei w, h, size, yuvsize, texsize, stridesize;
  // int     i, j;
  GLsizei   i,j;
  GLuint    byteformat;
  
  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> dt;
  
  // one of these ..
  format          =GL_BGRA;
  // format          =GL_RGBA;
  
  // internal_format =GL_RGBA;
  internal_format =GL_RGBA8;
  
  byteformat =GL_UNSIGNED_INT_8_8_8_8_REV;
  
  OpenGLContext ctx = OpenGLContext();
  
  ctx.loadExtensions();
  win=ctx.createWindow();
  ctx.makeCurrent(win);
  
  YUVBlockShader *shader = new YUVBlockShader();
  
  ctx.reserve(shader); // reserve stuff .. and communicate with the shader about the whereabouts of that stuff
  
  // w               =1920;
  // h               =1080;
  
  w               =1280;
  h               =720;
  
  size            =w*h;  // single plane size
  yuvsize         =(3*size)/2; // all planes in yuv
  stridesize      =w*4; /// one BGRA line
  texsize         =size*4; // BGRA
  
  image   = new GLubyte[yuvsize];
  y_image = new GLubyte[size];
  u_image = new GLubyte[size/4];
  v_image = new GLubyte[size/4];
  
  // rgb : w*h*3
  // yuv planes : 1 + 2*(1/4) = 1+1/2 = 3/2 = (3/2) * w*h 
  
  // load the image
  std::cout << "read " << readbytes("1.yuv",image) <<" bytes" << std::endl;
  std::cout << "should be " << yuvsize << " bytes" << std::endl;
  
  memcpy(y_image, image,              size  );
  memcpy(u_image, &image[size],       size/4);
  memcpy(v_image, &image[(5*size)/4], size/4); // 4/4 + 1/4 = 5/4
  
  // return;
  
  // let's reserve PBOs
  // getPBO(y_pbo,size,   y_payload);
  // getPBO(u_pbo,size/4, u_payload);
  // getPBO(v_pbo,size/4, v_payload);
  
  getPBO(pbo,texsize,payload);
  // let's create a dummy payload for comparison
  dummypayload = new GLubyte[texsize];
  
  // let's create the texture
  glEnable(GL_TEXTURE_2D);
  
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, byteformat, 0); 
  glBindTexture(GL_TEXTURE_2D, 0); // unbind
  
  //memset(payload,0,texsize);
  //memcpy(y_payload, y_image,  size  );
  //memcpy(u_payload, u_image,  size/4);
  //memcpy(v_payload, v_image,  size/4); // 4/4 + 1/4 = 5/4
  
  start = std::chrono::system_clock::now();
  for(i=0;i<h;i++) { // i == luma pixel index
    for(j=0;j<stridesize;j=j+4) { // 0, 4, 8, .. // j/4 == luma pixel index
      dummypayload[i*(stridesize)+j  ] =y_image[i*w    +j/4];     // b
      dummypayload[i*(stridesize)+j+1] =u_image[(i/2)*(w/2)+j/8]; // g
      dummypayload[i*(stridesize)+j+2] =v_image[(i/2)*(w/2)+j/8]; // r
      dummypayload[i*(stridesize)+j+3] =255;                      // a
    }
  }
  end = std::chrono::system_clock::now();
  dt = end-start;
  std::cout << "memory manipulation took " << dt.count()*1000 << " ms" << std::endl; // 66 ms 
  
  start = std::chrono::system_clock::now();
  memcpy(payload,dummypayload,texsize); // hd-ready : 4 ms
  end = std::chrono::system_clock::now();
  dt = end-start;
  std::cout << "memory upload took " << dt.count()*1000 << " ms" << std::endl;
  
  /*
  start = std::chrono::system_clock::now();
  for(i=0;i<h;i++) { // i == luma pixel index
    for(j=0;j<stridesize;j=j+4) { // 0, 4, 8, .. // j/4 == luma pixel index
      payload[i*(stridesize)+j  ] =y_image[i*w    +j/4];     // b
      payload[i*(stridesize)+j+1] =u_image[(i/2)*(w/2)+j/8]; // g
      payload[i*(stridesize)+j+2] =v_image[(i/2)*(w/2)+j/8]; // r
      payload[i*(stridesize)+j+3] =255;                      // a
    }
  }
  end = std::chrono::system_clock::now();
  dt = end-start;
  std::cout << "direct memory upload took " << dt.count()*1000 << " ms" << std::endl;
  */
  
  /*
  std::cout << "MAX " << (i*w)/4+j/8 << std::endl;
  
  for(i=0;i<=10;i++) {
    std::cout << "u> " << int(u_image[i]) << std::endl;
  }
  
  std::cout << std::endl;
  
  for(i=0;i<=10;i++) {
    std::cout << "v> " << int(v_image[i]) << std::endl;
  }
  
  std::cout << std::endl;
  
  for(i=0;i<100;i++) {
    std::cout << "> " << int(payload[i]) << std::endl;
  }
  */
  
  sleep_for(0.5s); // give it time to upload
  
  for(i=0;i<10;i++) {
    start = std::chrono::system_clock::now();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBindTexture(GL_TEXTURE_2D, tex); // this is the texture we will manipulate
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, byteformat, 0); // copy from pbo to texture 
    glBindTexture(GL_TEXTURE_2D, 0); // unbind
    
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind // important!
    glBindTexture(GL_TEXTURE_2D, 0); // unbind
    
    glFlush();
    glFinish();
    end = std::chrono::system_clock::now();
    
    dt = end-start;
    std::cout << "pbo => tex took " << dt.count()*1000 << " ms" << std::endl;
  }
    
  ctx.renderYUVBlockShader(win, shader, tex);
  
  sleep_for(5s);
  
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
      test_4();
      break;
    case(5):
      test_5();
      break;
    case(6):
      // test_6();
      break; 
    default:
      std::cout << "No such test "<<argcv[1]<<" for "<<argcv[0]<<std::endl;
  }
}
