#pragma once
namespace cold::modules::entityshader {
constexpr const char* mask=R"GLSL(#version 330 core
in vec2 vUv;out vec4 fragColor;uniform sampler2D uOriginal;
// Vanilla Sobel and blur scale RGB independently of alpha; only chromaticity survives.
float selected(vec4 value){float energy=dot(value.rgb,value.rgb);if(value.a<=1./255.||energy<=.0001)return 0.;return 1.-step(.04,distance(value.rgb*inversesqrt(energy),normalize(vec3(13,241,93))));}
void main(){vec4 value=texture(uOriginal,vUv);float mask=selected(value)*value.a;fragColor=vec4(mask,mask,mask,1);}
)GLSL";
constexpr const char* cull=R"GLSL(#version 330 core
out vec4 fragColor;uniform sampler2D uMask;
void main(){
 ivec2 start=ivec2(gl_FragCoord.xy)*8,size=textureSize(uMask,0);float occupied=0;
 for(int y=0;y<8;++y)for(int x=0;x<8;++x){ivec2 point=start+ivec2(x,y);if(all(lessThan(point,size)))occupied=max(occupied,texelFetch(uMask,point,0).r);}
 fragColor=vec4(occupied,occupied,occupied,1);
}
)GLSL";
constexpr const char* compose=R"GLSL(#version 330 core
in vec2 vUv;out vec4 fragColor;uniform sampler2D uOriginal,uMask,uGlow,uCull;uniform vec2 uPixel;uniform vec4 uColour;uniform float uWidth,uStrength,uOpacity;uniform int uMode,uGlowMode;uniform vec4 uTone,uShimmer;uniform vec2 uPulse;
// Vanilla Sobel and blur scale RGB independently of alpha; only chromaticity survives.
float selected(vec4 value){float energy=dot(value.rgb,value.rgb);if(value.a<=1./255.||energy<=.0001)return 0.;return 1.-step(.04,distance(value.rgb*inversesqrt(energy),normalize(vec3(13,241,93))));}
void main(){
 vec4 original=texture(uOriginal,vUv);vec4 retained=vec4(0);
 // This target is an effect mask, not scene colour. Retaining any unselected texel leaks
 // Minecraft's opaque black support pixels into the composite as a dark model outline.
 float blur=uMode==0?0:texture(uGlow,vUv).r;
 bool nearby=false;
 if(uMode!=1){
  ivec2 cell=ivec2(gl_FragCoord.xy)/8,size=textureSize(uCull,0);
  // The outline radius is bounded at eight pixels; adjacent max-pooled tiles cover every possible sample.
  for(int y=-1;y<=1&&!nearby;++y)for(int x=-1;x<=1&&!nearby;++x){ivec2 point=cell+ivec2(x,y);if(all(greaterThanEqual(point,ivec2(0)))&&all(lessThan(point,size)))nearby=texelFetch(uCull,point,0).r>0;}
 }
 if(!nearby&&blur<.0001&&retained.a<.001){fragColor=vec4(0);return;}
 float outline=0;
 if(nearby)for(int index=0;index<16;++index){float angle=float(index)*6.28318530718/16.;vec2 direction=vec2(cos(angle),sin(angle));for(int stepIndex=0;stepIndex<=8;++stepIndex){if(float(stepIndex)>uWidth)break;outline=max(outline,texture(uMask,vUv+direction*float(stepIndex)*uPixel).r);}}
 // Nightx colour, falloff, pulse and sweep equations operate on the available edge texture.
 float sharp=texture(uMask,vUv).r;
 float halo=uGlowMode==0?max(0.,blur-sharp):uGlowMode==1?max(0.,sharp-blur):abs(blur-sharp);
 float glow=uMode==0?0:pow(clamp(halo,0.,1.),uTone.x)*uStrength;
 float pulse=1.+uPulse.x*sin(uPulse.y*6.2831853);
 vec3 tint=clamp(mix(vec3(dot(uColour.rgb,vec3(.2126,.7152,.0722))),uColour.rgb,uTone.y),0.,1.);
 vec3 rimTint=mix(tint,vec3(1.),uTone.z);
 float rim=outline*uTone.w;
 float energy=max(rim,glow)*pulse;
 vec3 effect=(tint*glow+rimTint*rim)/max(glow+rim,.0001);
 if(uShimmer.x>.5){float sweep=1.-smoothstep(0.,uShimmer.z,abs(vUv.y-uShimmer.y));effect+=vec3(sweep*uShimmer.w);}
 float alpha=clamp(energy*uOpacity*uColour.a,0,1);
 float combined=alpha+retained.a*(1-alpha);vec3 colour=combined>.0001?(effect*alpha+retained.rgb*retained.a*(1-alpha))/combined:vec3(0);
 fragColor=vec4(colour,combined);
}
)GLSL";
}