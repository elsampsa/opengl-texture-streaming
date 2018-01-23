
# OpenGL texture upload test

We're testing here texture uploading to the GPU (no visualization, just upload).

Strategies include PBOs and TBOs.  Please, check it out and contribute!

Compile & link with:

    c++ --std=c++14 -I/usr/include/libdrm upload_pbo.cpp -lX11 -lGLEW -lGLU -lGL
 
Run with:

    ./a.out 1            Just test the glx infrastructure : creates a window
    ./a.out 2            Upload textures with PBOs - observe how different texture formats affect speed
    ./a.out 3            Tries to upload textures with TBOs - no luck

## Author

Sampsa Riikonen

## License

MIT

