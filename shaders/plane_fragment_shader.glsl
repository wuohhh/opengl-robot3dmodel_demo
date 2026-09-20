#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform vec3 lightColor;
uniform vec3 toyColor;
uniform float alpha;
void main()
{
    gl_FragColor =vec4(lightColor * toyColor,alpha);
}

