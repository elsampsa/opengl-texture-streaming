
# OpenGL texture upload test

We're testing here texture uploading to the GPU (no visualization, just upload) - strategies include PBOs and TBOs.

Compile & link with:

    c++ --std=c++14 -I/usr/include/libdrm upload_pbo.cpp -lX11 -lGLEW -lGLU -lGL
 
Run with:

    ./a.out 1            Just test the glx infrastructure : creates a window
    ./a.out 2            Upload textures with PBOs (just upload, no visualization)
    ./a.out 3            Tries to upload textures with TBOs - no luck
    ./a.out 4            Upload a YUV image (using GL_RED), interpolate to RGB on gpu, show the image.
    ./a.out 5            Upload a YUV image (using GL_RGBA), interpolate to RGB on gpu, show the image.

## Author

Sampsa Riikonen

## License

MIT

