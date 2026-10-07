#include "metal_display.hpp"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <limits>
namespace outrun::mac {
struct MetalDisplay::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLRenderPipelineState> pipeline;
    id<MTLTexture> input,output;
    NSWindow* window;
    CAMetalLayer* layer;
    unsigned width{},height{},input_width{},input_height{};
};
MetalDisplay::MetalDisplay():impl_(std::make_unique<Impl>()){}
MetalDisplay::~MetalDisplay(){[impl_->window close];}
static bool fail(std::string& error,NSString* message){error=message?message.UTF8String:"Metal operation failed";return false;}
bool MetalDisplay::open(unsigned width,unsigned height,bool visible,std::string& error){
    @autoreleasepool {
        auto& p=*impl_;
        if(p.device)return fail(error,@"Display already open");
        if(!width||!height||width>8192||height>8192)return fail(error,@"Invalid display dimensions");
        p.device=MTLCreateSystemDefaultDevice();
        if(!p.device)return fail(error,@"No Metal device available");
        p.queue=[p.device newCommandQueue];
        if(!p.queue)return fail(error,@"Cannot create Metal command queue");
        // Runtime MSL compilation works with Command Line Tools alone.
        NSString* source=@"#include <metal_stdlib>\nusing namespace metal;\n"
        "struct V { float4 p [[position]]; float2 uv; };\n"
        "vertex V vs(uint i [[vertex_id]]) { float2 a[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; V v; v.p=float4(a[i],0,1); v.uv=float2((a[i].x+1)*0.5,1-(a[i].y+1)*0.5); return v; }\n"
        "fragment float4 fs(V v [[stage_in]],texture2d<float> image [[texture(0)]]) { constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest); return image.sample(s,v.uv); }\n";
        NSError* e=nil;
        MTLCompileOptions* options=[MTLCompileOptions new];options.fastMathEnabled=NO;
        id<MTLLibrary> lib=[p.device newLibraryWithSource:source options:options error:&e];
        if(!lib)return fail(error,e.localizedDescription);
        MTLRenderPipelineDescriptor* d=[MTLRenderPipelineDescriptor new];
        d.vertexFunction=[lib newFunctionWithName:@"vs"];d.fragmentFunction=[lib newFunctionWithName:@"fs"];
        d.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
        p.pipeline=[p.device newRenderPipelineStateWithDescriptor:d error:&e];
        if(!p.pipeline)return fail(error,e.localizedDescription);
        p.width=width;p.height=height;
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;
        p.output=[p.device newTextureWithDescriptor:td];
        if(!p.output)return fail(error,@"Cannot allocate render target");
        if(visible){
            if(![NSThread isMainThread])return fail(error,@"Window must be created on main thread");
            [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            p.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,width,height) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
            p.window.releasedWhenClosed=NO;p.window.title=@"OutRunMac — validation Metal";
            p.layer=[CAMetalLayer layer];p.layer.device=p.device;p.layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
            p.layer.framebufferOnly=YES;
            p.window.contentView.wantsLayer=YES;p.window.contentView.layer=p.layer;
            [p.window center];[p.window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
        }
        return true;
    }
}
bool MetalDisplay::present(const std::uint8_t* bytes,unsigned width,unsigned height,std::string& error){
    @autoreleasepool {
        auto& p=*impl_;
        if(!p.pipeline||!bytes||!width||!height||width>8192||height>8192)return fail(error,@"Invalid RGBA frame or unopened display");
        if(!p.input||p.input_width!=width||p.input_height!=height){
            auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
            td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
            p.input=[p.device newTextureWithDescriptor:td];p.input_width=width;p.input_height=height;
            if(!p.input)return fail(error,@"Cannot allocate frame texture");
        }
        [p.input replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0 withBytes:bytes bytesPerRow:std::size_t(width)*4];
        id<CAMetalDrawable> drawable=nil;
        if(p.window){
            NSSize size=[p.window.contentView convertSizeToBacking:p.window.contentView.bounds.size];
            if(size.width<1||size.height<1)return fail(error,@"Window has no drawable area");
            p.layer.drawableSize=CGSizeMake(size.width,size.height);
            drawable=[p.layer nextDrawable];
            if(!drawable)return fail(error,@"Metal drawable unavailable");
        }
        id<MTLTexture> target=drawable?drawable.texture:p.output;
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
        id<MTLCommandBuffer> command=[p.queue commandBuffer];
        id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
        if(!command||!encoder)return fail(error,@"Cannot encode Metal frame");
        const double scale=std::min(double(target.width)/width,double(target.height)/height);
        MTLViewport viewport{(target.width-width*scale)/2,(target.height-height*scale)/2,width*scale,height*scale,0,1};
        [encoder setViewport:viewport];[encoder setRenderPipelineState:p.pipeline];[encoder setFragmentTexture:p.input atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
        if(drawable)[command presentDrawable:drawable];
        [command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)return fail(error,command.error.localizedDescription);
        return true;
    }
}
bool MetalDisplay::readback(std::vector<std::uint8_t>& bytes,std::string& error){
    auto& p=*impl_;
    if(!p.output||p.window)return fail(error,@"Readback requires an offscreen display");
    bytes.resize(std::size_t(p.width)*p.height*4);
    [p.output getBytes:bytes.data() bytesPerRow:std::size_t(p.width)*4 fromRegion:MTLRegionMake2D(0,0,p.width,p.height) mipmapLevel:0];
    return true;
}
bool MetalDisplay::running(){
    if(!impl_->window)return false;
    @autoreleasepool { NSEvent* e;
        while((e=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])){
            if(e.type==NSEventTypeKeyDown&&e.keyCode==53){[impl_->window close];break;}
            [NSApp sendEvent:e];
        }
        [NSApp updateWindows];return impl_->window.visible;
    }
}
std::string MetalDisplay::device_name()const{return impl_->device?impl_->device.name.UTF8String:"";}
void* MetalDisplay::native_window()const{return (__bridge void*)impl_->window;}
}
