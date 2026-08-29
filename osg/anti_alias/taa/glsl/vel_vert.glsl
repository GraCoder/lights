
#version 430 compatibility

layout(binding = 0) uniform CameraBufferObject{
	mat4 pre_matrix;
	mat4 cur_matrix;
} camera_buffer;

struct VelocityStep{
	vec4 pre_pos;
	vec4 cur_pos;
};

out vec4 clr;
out VelocityStep vel_out;

uniform mat4 taa_proj;

void main()
{
	gl_Position = taa_proj * gl_ModelViewMatrix * gl_Vertex; 
  //gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	clr = gl_Color;

	vel_out.pre_pos = camera_buffer.pre_matrix * gl_Vertex;
	vel_out.cur_pos = camera_buffer.cur_matrix * gl_Vertex;	
}
