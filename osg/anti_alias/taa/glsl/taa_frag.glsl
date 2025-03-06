
#version 330 compatibility

in vec2 uv;

uniform bool init_clr;
uniform vec2 texture_size;
uniform vec2 taa_jitter;

uniform sampler2D dep_tex;
uniform sampler2D vel_tex;
uniform sampler2D pre_tex;
uniform sampler2D cur_tex;

const ivec2 sample_offset[9] = ivec2[9](
	ivec2(-1, -1),
	ivec2( 0, -1),
	ivec2( 1, -1),
	ivec2(-1,  0),
	ivec2( 0,  0),
	ivec2( 1,  0),
	ivec2(-1,  1),
	ivec2( 0,  1),
	ivec2( 1,  1)
);

vec2 get_closest(vec2 uv)
{
	vec2 close_uv = uv;
	float close_dep = 1.f;
	vec2 delta_res = texture_size.xy;	
	for(int i = 0; i < 9; i++){
    vec2 uvtmp = uv + delta_res * sample_offset[i]; 
    float deptmp = texture(dep_tex, uvtmp).r;
		if(deptmp < close_dep)
		{
			close_dep = deptmp;
			close_uv = uvtmp;	
		}
	}
	return close_uv;
}

vec3 rgb2YCoCg(vec3 rgb)
{
	vec3 ycocg;
	ycocg.y = (rgb.r - rgb.b) * 2;
  float tmp = rgb.r + rgb.b;
	ycocg.z = rgb.g * 2 - tmp;
	ycocg.x = ycocg.z + tmp + tmp;
	return ycocg;	
}

vec3 YCoCg2rgb(vec3 ycocg)
{
  vec3 tmp = ycocg * 0.25;
	return vec3(tmp.x + tmp.y - tmp.z, tmp.x + tmp.z, tmp.x - tmp.y - tmp.z); 
}

float luminace(vec3 color)
{
	return 0.25 * color.r + 0.5 * color.g + 0.25 * color.b;
}

vec3 tone_map(vec3 color)
{
	return color / (1 + luminace(color));
}

vec3 untone_map(vec3 color)
{
	return color / (1 - luminace(color));
}

vec3 clip_aabb(vec3 cur_clr, vec3 pre_clr, vec2 cur_uv)
{
  vec3 aabb_min = cur_clr, aabb_max = cur_clr;
  vec2 delta_res = texture_size.xy; 
  vec3 m1 = vec3(0), m2 = vec3(0);
	for(int i = 0; i < 9; i++){
    vec3 c = rgb2YCoCg(tone_map(textureLodOffset(cur_tex, cur_uv, 0, sample_offset[i]).rgb));
		aabb_min = min(aabb_min, c);
		aabb_max = max(aabb_max, c);
  }

  vec3 p_clip = 0.5 * (aabb_max + aabb_min);
  vec3 e_clip = 0.5 * (aabb_max - aabb_min);

  vec3 v_clip = pre_clr - p_clip;
  vec3 v_unit = v_clip.xyz / e_clip;
  vec3 a_unit = abs(v_unit);
  float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

  if (ma_unit > 1.0)
    pre_clr = p_clip + v_clip / ma_unit;

  return pre_clr;
}


vec3 clamp_aabb(vec3 cur_clr, vec3 pre_clr, vec2 cur_uv)
{
  vec3 aabb_min = cur_clr, aabb_max = cur_clr;
  vec2 delta_res = texture_size.xy; 
	for(int i = 0; i < 9; i++){
		vec2 uvtmp = cur_uv + delta_res * sample_offset[i];
		vec3 c = rgb2YCoCg(tone_map(texture(cur_tex, uvtmp).rgb));
		aabb_min = min(aabb_min, c);
		aabb_max = max(aabb_max, c);
  }
	return clamp(pre_clr, aabb_min, aabb_max);
}


void main()
{
	vec3 cur_clr = texture(cur_tex, uv).rgb;

  if(init_clr){
    gl_FragColor = vec4(cur_clr, 1);
    return;
  }

	vec2 cur_uv = uv - taa_jitter;

	vec2 velocity = texture(vel_tex, get_closest(uv)).rg;
	//vec2 velocity = texture(vel_tex, uv).rg;
	vec2 vel_uv = clamp(uv - velocity, 0, 1);

	vec3 pre_clr = texture(pre_tex, vel_uv).rgb;

	vec3 cur_ycg = rgb2YCoCg(tone_map(cur_clr));
	vec3 pre_ycg = rgb2YCoCg(tone_map(pre_clr));
	//pre_clr = untone_map(YCoCg2rgb(clamp_aabb(cur_ycg, pre_ycg, uv)));	
	pre_clr = untone_map(YCoCg2rgb(clip_aabb(cur_ycg, pre_ycg, cur_uv)));	

  float factor = clamp(0.05 + length(velocity) * 100, 0, 1);
	//factor = 0.05;

	vec3 des_clr = mix(pre_clr, cur_clr, factor); 
	gl_FragColor = vec4(des_clr, 1);
}