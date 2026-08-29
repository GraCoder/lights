#include "testNode.h"

#include <format>
//#include <boost/filesystem.hpp>
#include <filesystem>
#include <fstream>

#include <osg/Geometry>
#include <osgUtil/CullVisitor>

#include "inc/common.h"

const std::string vertShader = R"(

#version 330 compatibility

out vec4 clr;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	clr = gl_Color;
}

)";

const std::string fragShader = R"(

#version 330 compatibility

in vec4 clr;

void main()
{
	vec4 clr_tmp = clr;
	clr_tmp.w = 0.213 * clr.r + 0.715 * clr.g + 0.072 * clr.b;
	gl_FragColor = clr_tmp;
}

)";

const std::string svShader = R"(

#version 330 compatibility

out vec4 clr;

void main()
{
	gl_Position = gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	clr = gl_Color;
}

)";

constexpr char fxaaShader[] = R"(

#version 330 compatibility

uniform vec2 tex_size;
uniform sampler2D clr_tex;
uniform sampler2D dep_tex;

const int search_step = 10;
const int guess = 8;
const float contrast_threshold = 0.0312; //0.0312-0.0833
const float relcontrast_threshold = 0.063; //0.063-0.333

float luminace(vec3 clr)
{
	return 0.213 * clr.r + 0.715 * clr.g + 0.072 * clr.b;
}

void main()
{
	vec2 uv = gl_TexCoord[0].xy;
	vec2 des = vec2(1 / tex_size.x, 1 / tex_size.y);
	float n = texture(clr_tex, uv + vec2(0,  des.y)).w; 
	float s = texture(clr_tex, uv + vec2(0, -des.y)).w; 
	float w = texture(clr_tex, uv + vec2(-des.x, 0)).w; 
	float e = texture(clr_tex, uv + vec2( des.x, 0)).w; 
	float c = texture(clr_tex, uv).w; 

	float max_lu = max(c, max(max(n, s), max(w, e)));
	float min_lu = min(c, min(min(n, s), min(w, e)));
	float constract = max_lu - min_lu;
	if(constract < max(contrast_threshold, max_lu * relcontrast_threshold)){
		gl_FragColor = vec4(texture(clr_tex, uv).rgb, 1);
		return;
	}

	float nw = texture(clr_tex, uv + vec2(-des.x,  des.y)).w; 
	float sw = texture(clr_tex, uv + vec2(-des.x, -des.y)).w; 
	float se = texture(clr_tex, uv + vec2( des.x, -des.y)).w; 
	float ne = texture(clr_tex, uv + vec2( des.x,  des.y)).w; 
	
	float filter = (2 * (n + s + w + e) + nw + sw + se + ne) / 12;
	filter = clamp(abs(filter - c) / constract, 0, 1);
	float pix_blend = smoothstep(0, 1, filter);
	pix_blend = pix_blend * pix_blend;

	float vertical = abs(n + s - 2 * c) * 2 + abs(ne + se - 2 * e) + abs(nw + sw - 2 * w);
	float horizontal = abs(e + w - 2 * c) * 2 + abs(ne + nw - 2 * n) + abs(se + sw - 2 * s);

	bool is_hor = vertical > horizontal;
	vec2 pix_step = vec2(des.x, 0);
	if(is_hor)
		pix_step = vec2(0, des.y);	
	float positive = abs((is_hor ? n : e) - c);
	float negative = abs((is_hor ? s : w) - c);

	float gradient, opposite;
	if(positive > negative){
		gradient = positive;
		opposite = is_hor ? n : e;
	}
	else{
		pix_step = -pix_step;
		gradient = negative;
		opposite = is_hor ? s : w;
	}
	//gl_FragColor = vec4(texture(clr_tex, uv + pix_step * pix_blend).rgb, 1);
	//return;
	
	vec2 uvedge = uv;
	uvedge += pix_step * 0.5;
	vec2 edge_step = is_hor ? vec2(des.x, 0) : vec2(0, des.y);
	
	float edge_lu = (c + opposite) * 0.5;
	float gradient_threshold = gradient * 0.25;
	float p_lu_delta, n_lu_delta, p_dis, n_dis;
	int i = 0;
	for(i = 1; i < search_step; i++){
		p_lu_delta = texture(clr_tex, uvedge + i * edge_step).w - edge_lu;
		if(abs(p_lu_delta) > gradient_threshold){
			p_dis = i * (is_hor ? edge_step.x : edge_step.y);
			break;
		}
	}
	if(i == search_step)
		p_dis = (is_hor ? edge_step.x : edge_step.y) * guess;
	for(i = 1; i < search_step; i++){
		n_lu_delta = texture(clr_tex, uvedge - i * edge_step).w - edge_lu;
		if(abs(n_lu_delta) > gradient_threshold){
			n_dis = i * (is_hor ? edge_step.x : edge_step.y);
			break;
		}
	}
	if(i == search_step)
		n_dis = (is_hor ? edge_step.x : edge_step.y) * guess;

	float edge_blend;
	if(p_dis < n_dis){
		if(sign(p_lu_delta) == sign(c - edge_lu))
			edge_blend = 0;
		else
			edge_blend = 0.5 - p_dis / (p_dis + n_dis);
	}
	else{
		if(sign(n_lu_delta) == sign(c - edge_lu))
			edge_blend = 0;
		else
			edge_blend = 0.5 - n_dis / (p_dis + n_dis);
	}
	float final_blend = max(pix_blend, edge_blend);	

	//vec3 clr1 = texture(clr_tex, uv).rgb;
	//vec3 clr2 = texture(clr_tex, uv + pix_step).rgb;
	//gl_FragColor = vec4(mix(clr1, clr2, final_blend), 1); 
	gl_FragColor.xyz = texture(clr_tex, uv + pix_step * final_blend).xyz;
	gl_FragColor.w = 1;
}

)";

TestNode::TestNode()
{
  setCullingActive(false);
  init();
}

void TestNode::init()
{
  //_quad = createTexturedQuadGeometry(Vec3(0, -0.5, 0), Vec3(0.5, 0.5, 0), Vec3(-0.5, 0.5, 0));
  _quad = createTexturedQuadGeometry(Vec3(-0.5, -0.5, 0), Vec3(1, 0, 0), Vec3(0, 1, 0));
  auto clr = new Vec3Array;
  clr->push_back(Vec3(1, 1, 1));
  // clr->push_back(Vec3(0, 1, 0));
  // clr->push_back(Vec3(0, 0, 1));
  // clr->push_back(Vec3(1, 1, 0));
  _quad->setColorArray(clr, Array::BIND_OVERALL);
  auto ss = _quad->getOrCreateStateSet();
  auto prg = new osg::Program;
  prg->addShader(new osg::Shader(Shader::VERTEX, vertShader));
  prg->addShader(new osg::Shader(Shader::FRAGMENT, fragShader));
  ss->setAttribute(prg);
  _quad->setCullingActive(false);

  _cam = new Camera;
  _cam->addChild(_quad);
  _cam->setRenderOrder(Camera::PRE_RENDER);
  _cam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  _cam->setClearColor(osg::Vec4(0, 0, 0, 0));
  _clrTex = new osg::Texture2D;
  _clrTex->setInternalFormat(GL_RGBA);
  _clrTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _clrTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
  _depTex = new osg::Texture2D;
  _depTex->setInternalFormat(GL_DEPTH_COMPONENT);
  _depTex->setFilter(Texture::MIN_FILTER, Texture::NEAREST);
  _depTex->setFilter(Texture::MAG_FILTER, Texture::NEAREST);
  _cam->attach(Camera::COLOR_BUFFER, _clrTex);
  _cam->attach(Camera::DEPTH_BUFFER, _depTex);
  _cam->setRenderTargetImplementation(_cam->FRAME_BUFFER_OBJECT);

  _ssquad = createTexturedQuadGeometry(Vec3(-1, -1, 0), Vec3(2, 0, 0), Vec3(0, 2, 0));
  ss = _ssquad->getOrCreateStateSet();
  ss->setTextureAttributeAndModes(0, _clrTex);
  ss->setTextureAttributeAndModes(1, _depTex);
  ss->getOrCreateUniform("clr_tex", Uniform::SAMPLER_2D)->set(0);
  ss->getOrCreateUniform("dep_tex", Uniform::SAMPLER_2D)->set(1);
  _ssquad->setCullingActive(false);
  {
    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, svShader));
    prg->addShader(new osg::Shader(Shader::FRAGMENT, fxaaShader));
    ss->setAttribute(prg);
  }
  addChild(_ssquad);
}

void TestNode::traverse(NodeVisitor& nv)
{
  osg::Group::traverse(nv);

  auto cv = nv.asCullVisitor();

  if (cv) {
    auto vp = cv->getViewport();
    if (_clrTex->getTextureWidth() != vp->width() || _clrTex->getTextureHeight() != vp->height()) {
      _clrTex->setTextureSize(vp->width(), vp->height());
      _clrTex->dirtyTextureObject();
      _depTex->setTextureSize(vp->width(), vp->height());
      _depTex->dirtyTextureObject();
      _cam->dirtyAttachmentMap();

      auto ss = _ssquad->getOrCreateStateSet();
      ss->getOrCreateUniform("tex_size", osg::Uniform::FLOAT_VEC2)->set(osg::Vec2(vp->width(), vp->height()));
    }
    _cam->accept(*cv);
  }
}
