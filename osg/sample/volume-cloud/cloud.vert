#version 330

out vec3 localVertex;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	localVertex = gl_Vertex.xyz;
}
