#include "wallclock/renderer.h"
#include <stdexcept>
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS 8192
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace wc {
Image load_image(const std::string& path) {
    int w=0,h=0,n=0;
    if(!stbi_info(path.c_str(),&w,&h,&n)) throw std::runtime_error("Cannot inspect image: "+path);
    if(w<1 || h<1 || w>8192 || h>8192 || 1LL*w*h>34000000) throw std::runtime_error("Image exceeds size limit: "+path);
    unsigned char* data=stbi_load(path.c_str(),&w,&h,&n,4);
    if(!data) throw std::runtime_error("Cannot decode image: "+path);
    Image image;
    try { image.width=w; image.height=h; image.pixels.assign(data,data+size_t(w)*h*4); }
    catch(...) { stbi_image_free(data); throw; }
    stbi_image_free(data); return image;
}
void write_image(const Image& im,const std::string& path) {
    auto dot=path.find_last_of('.'); std::string ext=dot==std::string::npos?"":path.substr(dot);
    int ok=0;
    if(ext==".png") ok=stbi_write_png(path.c_str(),im.width,im.height,4,im.pixels.data(),im.width*4);
    else if(ext==".bmp") ok=stbi_write_bmp(path.c_str(),im.width,im.height,4,im.pixels.data());
    else throw std::runtime_error("Output extension must be .png or .bmp");
    if(!ok) throw std::runtime_error("Cannot write image: "+path);
}
}
