#pragma once
#include "core.h"
#include <functional>

namespace wc {
struct Image {
    int width=0, height=0;
    std::vector<unsigned char> pixels; // straight RGBA
    Image() = default;
    Image(int w,int h,Color fill);
    void blend(int x,int y,Color color,double coverage=1);
};
Image load_image(const std::string& path);
void write_image(const Image& image,const std::string& path); // PNG or BMP
struct RenderContext { const std::tm& time; int width, height; double unit; };
// Register trusted compiled-in renderers; no scripts, dynamic loading or OS access.
using ElementRenderer = std::function<void(Image&,const Element&,const RenderContext&)>;
class Renderer {
public:
    Renderer();
    void register_element(const std::string& kind,ElementRenderer render);
    Image render(const Scene& scene,const std::tm& time,int width,int height);
private:
    std::map<std::string,ElementRenderer> elements_;
    Image background_;
    std::string background_key_;
};
}
