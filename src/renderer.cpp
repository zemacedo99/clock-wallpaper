#include "wallclock/renderer.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace wc {
Image::Image(int w,int h,Color c):width(w),height(h),pixels(size_t(w)*h*4) {
    for(size_t i=0;i<pixels.size();i+=4) {pixels[i]=c.r;pixels[i+1]=c.g;pixels[i+2]=c.b;pixels[i+3]=c.a;}
}
void Image::blend(int x,int y,Color c,double coverage) {
    if(x<0 || y<0 || x>=width || y>=height) return;
    size_t i=(size_t(y)*width+x)*4; double a=c.a/255.0*std::max(0.0,std::min(1.0,coverage));
    pixels[i]=static_cast<unsigned char>(pixels[i]*(1-a)+c.r*a+.5);
    pixels[i+1]=static_cast<unsigned char>(pixels[i+1]*(1-a)+c.g*a+.5);
    pixels[i+2]=static_cast<unsigned char>(pixels[i+2]*(1-a)+c.b*a+.5);
    pixels[i+3]=255;
}
namespace {
constexpr double pi=3.14159265358979323846;
void line(Image& im,double ax,double ay,double bx,double by,double radius,Color c) {
    int x0=std::max(0,int(std::floor(std::min(ax,bx)-radius-1)));
    int x1=std::min(im.width-1,int(std::ceil(std::max(ax,bx)+radius+1)));
    int y0=std::max(0,int(std::floor(std::min(ay,by)-radius-1)));
    int y1=std::min(im.height-1,int(std::ceil(std::max(ay,by)+radius+1)));
    double dx=bx-ax,dy=by-ay,d=dx*dx+dy*dy;
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
        double t=d?std::max(0.0,std::min(1.0,((x+.5-ax)*dx+(y+.5-ay)*dy)/d)):0;
        double xx=x+.5-ax-t*dx, yy=y+.5-ay-t*dy;
        im.blend(x,y,c,radius+.5-std::sqrt(xx*xx+yy*yy));
    }
}
void circle(Image& im,double x,double y,double radius,Color c) {line(im,x,y,x,y,radius,c);}
Color sample(const Image& im,double x,double y) {
    x=std::max(0.0,std::min(double(im.width-1),x));y=std::max(0.0,std::min(double(im.height-1),y));
    int x0=int(x),y0=int(y),x1=std::min(x0+1,im.width-1),y1=std::min(y0+1,im.height-1);
    double fx=x-x0,fy=y-y0; unsigned char channels[4];
    for(int k=0;k<4;++k) channels[k]=static_cast<unsigned char>(
        (im.pixels[(size_t(y0)*im.width+x0)*4+k]*(1-fx)+im.pixels[(size_t(y0)*im.width+x1)*4+k]*fx)*(1-fy)+
        (im.pixels[(size_t(y1)*im.width+x0)*4+k]*(1-fx)+im.pixels[(size_t(y1)*im.width+x1)*4+k]*fx)*fy+.5);
    return Color{channels[0],channels[1],channels[2],channels[3]};
}
void sprite(Image& im,const Image& src,double x,double y,double length,double degrees,double px,double py) {
    double scale=length/std::max(src.width,src.height),a=degrees*pi/180,cs=std::cos(a),sn=std::sin(a);
    double x0=im.width,y0=im.height,x1=0,y1=0;
    for(int i=0;i<4;++i) {
        double xx=((i&1?src.width:0)-px*src.width)*scale;
        double yy=((i&2?src.height:0)-py*src.height)*scale;
        double rx=x+cs*xx-sn*yy,ry=y+sn*xx+cs*yy;
        x0=std::min(x0,rx);x1=std::max(x1,rx);y0=std::min(y0,ry);y1=std::max(y1,ry);
    }
    for(int yy=std::max(0,int(std::floor(y0)));yy<std::min(im.height,int(std::ceil(y1)));++yy)
        for(int xx=std::max(0,int(std::floor(x0)));xx<std::min(im.width,int(std::ceil(x1)));++xx) {
            double dx=xx+.5-x,dy=yy+.5-y;
            double sx=(cs*dx+sn*dy)/scale+px*src.width-.5,sy=(-sn*dx+cs*dy)/scale+py*src.height-.5;
            if(sx>=0 && sy>=0 && sx<src.width && sy<src.height) im.blend(xx,yy,sample(src,sx,sy));
        }
}
// Small built-in 5x7 uppercase font, column bits from top to bottom. No font runtime.
const unsigned char letters[26][5]={
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {65,65,127,65,65},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67}};
const unsigned char digits[10][5]={
 {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},
 {39,69,69,69,57},{62,73,73,73,50},{1,113,9,5,3},{54,73,73,73,54},{38,73,73,73,62}};
void text(Image& im,const std::string& value,double x,double y,double height,Color color) {
    int scale=std::max(1,int(height/7));
    int origin=int(x)-int(value.size()*6-(!value.empty()))*scale/2;
    for(size_t n=0;n<value.size();++n) {
        unsigned char ch=static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(value[n])));
        const unsigned char* glyph=ch>='A'&&ch<='Z'?letters[ch-'A']:ch>='0'&&ch<='9'?digits[ch-'0']:nullptr;
        for(int col=0;col<5;++col) for(int row=0;row<7;++row) {
            bool on=glyph?(glyph[col]&(1<<row))!=0 : ch==':'?(col==2&&(row==2||row==5)):ch=='-'?row==3: false;
            if(on) for(int yy=0;yy<scale;++yy) for(int xx=0;xx<scale;++xx)
                im.blend(origin+int(n)*6*scale+col*scale+xx,int(y)-7*scale/2+row*scale+yy,color);
        }
    }
}
}
Renderer::Renderer() {
    register_element("markers",[](Image& im,const Element& e,const RenderContext& c){
        for(int i=0;i<e.count;++i) {
            double a=(360.0*i/e.count+e.offset)*pi/180;
            circle(im,e.x*c.width+std::sin(a)*e.radius*c.unit,e.y*c.height-std::cos(a)*e.radius*c.unit,e.size*c.unit/2,e.color);
        }
    });
    auto get_sprite=[this](const std::string& path)->const Image& {
        auto found=sprites_.find(path);
        if(found!=sprites_.end()) return found->second;
        Image decoded=load_image(path);
        if(decoded.pixels.size()>16*1024*1024) throw std::runtime_error("Sprite exceeds 16 MiB decoded budget");
        size_t bytes=decoded.pixels.size();for(const auto& item:sprites_) bytes+=item.second.pixels.size();
        if(bytes>16*1024*1024) sprites_.clear();
        return sprites_.emplace(path,std::move(decoded)).first->second;
    };
    register_element("hand",[get_sprite](Image& im,const Element& e,const RenderContext& c){
        double degrees=element_angle(e,c.time),a=degrees*pi/180;
        if(e.shape=="image") {
            sprite(im,get_sprite(e.image),e.x*c.width,e.y*c.height,e.length*c.unit,degrees,e.pivot_x,e.pivot_y);return;
        }
        double ax=e.x*c.width-std::sin(a)*e.tail*c.unit,ay=e.y*c.height+std::cos(a)*e.tail*c.unit;
        double bx=e.x*c.width+std::sin(a)*e.length*c.unit,by=e.y*c.height-std::cos(a)*e.length*c.unit;
        if(e.shape=="triangle") {
            // Short overlapping discs make a tapered pointer without a full-frame AA buffer.
            int steps=std::max(1,int((e.length+e.tail)*c.unit*2));
            for(int i=0;i<=steps;++i) {double f=double(i)/steps;circle(im,ax+(bx-ax)*f,ay+(by-ay)*f,e.width*c.unit/2*(1-f),e.color);}
        } else line(im,ax,ay,bx,by,e.width*c.unit/2,e.color);
    });
    register_element("orbit",[get_sprite](Image& im,const Element& e,const RenderContext& c){
        double degrees=element_angle(e,c.time),a=degrees*pi/180;
        double x=e.x*c.width+std::sin(a)*e.radius*c.unit,y=e.y*c.height-std::cos(a)*e.radius*c.unit;
        if(!e.image.empty()) {sprite(im,get_sprite(e.image),x,y,e.size*c.unit,e.rotate?degrees+e.heading_offset:0,e.pivot_x,e.pivot_y);return;}
        if(e.shape!="crescent") {circle(im,x,y,e.size*c.unit/2,e.color);return;}
        double r=e.size*c.unit/2;
        for(int yy=std::max(0,int(y-r-1));yy<std::min(im.height,int(y+r+2));++yy)
            for(int xx=std::max(0,int(x-r-1));xx<std::min(im.width,int(x+r+2));++xx) {
                double dx=xx+.5-x,dy=yy+.5-y;
                double outer=r+.5-std::sqrt(dx*dx+dy*dy);
                dx-=r*.45;dy+=r*.24;
                double cut=std::sqrt(dx*dx+dy*dy)-r*.86+.5;
                im.blend(xx,yy,e.color,std::min(outer,cut)); // Transparent cutout preserves photos.
            }
    });
    register_element("text",[](Image& im,const Element& e,const RenderContext& c){
        char formatted[512];std::string value=e.text;
        if(value.find('%')!=std::string::npos) {
            if(!std::strftime(formatted,sizeof(formatted),value.c_str(),&c.time)) throw std::runtime_error("Formatted text exceeds 511 bytes");
            value=formatted;
        }
        text(im,value,e.x*c.width,e.y*c.height,e.size*c.unit,e.color);
    });
}
void Renderer::register_element(const std::string& name,ElementRenderer render,ElementDeadline deadline) {
    if(name.empty() || !render || !deadline) throw std::runtime_error("Invalid layer callbacks");
    elements_[name]=std::move(render);
    deadlines_[name]=std::move(deadline);
}
void Renderer::validate(const Config& config) const {
    for(const auto& theme:config.themes) for(const auto& e:theme.second.elements)
        if(!elements_.count(e.kind)) throw std::runtime_error("Unknown layer kind: "+e.kind);
}
std::time_t Renderer::next_change(const Config& config,const Scene& scene,std::time_t now) const {
    Scene calendar=scene;calendar.theme.update_seconds=0;
    auto best=wc::next_change(config,calendar,now);
    for(const auto& e:scene.theme.elements) {
        auto found=deadlines_.find(e.kind);
        if(found==deadlines_.end()) throw std::runtime_error("No deadline callback for layer: "+e.kind);
        auto next=found->second(e,scene.theme,now);
        if(next && next<=now) throw std::runtime_error("Layer deadline must be strictly in the future: "+e.kind);
        if(next) best=std::min(best,next);
    }
    return best;
}
Image Renderer::render(const Scene& scene,const std::tm& time,int w,int h) {
    validate_size(w,h); const auto& theme=scene.theme;
    std::ostringstream key; key<<theme.background<<'|'<<int(theme.color.r)<<','<<int(theme.color.g)<<','<<int(theme.color.b)<<'|'<<w<<'x'<<h;
    if(key.str()!=background_key_) {
        Image next(w,h,theme.color);
        if(!theme.background.empty()) {
            Image source=load_image(theme.background);
            double scale=std::max(double(w)/source.width,double(h)/source.height);
            for(int y=0;y<h;++y) for(int x=0;x<w;++x)
                next.blend(x,y,sample(source,(x+.5-w/2.0)/scale+source.width/2.0-.5,(y+.5-h/2.0)/scale+source.height/2.0-.5));
        }
        background_=std::move(next); background_key_=key.str();
    }
    Image output=background_;
    RenderContext context{time,w,h,double(std::min(w,h))};
    for(const auto& element:theme.elements) {
        if(!element_visible(element,time)) continue;
        auto fn=elements_.find(element.kind);
        if(fn==elements_.end()) throw std::runtime_error("No renderer registered for element: "+element.kind);
        fn->second(output,element,context);
    }
    if(!theme.text.empty()) text(output,theme.text,w*.5,h*.87,std::min(w*.7/std::max(size_t(1),theme.text.size())*7/6,context.unit*.022),theme.text_color);
    return output;
}
}
