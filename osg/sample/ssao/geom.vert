#version 330 compatibility

out vec3 normal;
out vec3 pos;

void main()
{
	vec4 viewPos = gl_ModelViewMatrix * gl_Vertex;
	pos = viewPos.xyz;

	gl_TexCoord[0] = gl_MultiTexCoord0;

	normal  = transpose(inverse(mat3(gl_ModelViewMatrix))) *  gl_Normal;
	gl_Position = gl_ProjectionMatrix * viewPos;
}
