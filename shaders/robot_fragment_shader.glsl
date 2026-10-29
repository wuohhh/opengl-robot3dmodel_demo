#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

/** @brief 方向光反射计算使用的材质参数。 */
struct Material
{
    vec3 ambient;    ///< 材质对环境光的 RGB 反射系数。
    vec3 diffuse;    ///< 材质对漫反射光的 RGB 反射系数。
    vec3 specular;   ///< 材质对镜面高光的 RGB 反射系数。
    float shininess; ///< 镜面高光指数，用作 pow 的指数，控制高光集中程度。
};

/** @brief 方向光的传播方向及各类光照的 RGB 强度。 */
struct Light
{
    vec3 direction; ///< 光线传播方向，计算入射光方向时取反并归一化。

    vec3 ambient;  ///< 环境光的 RGB 强度。
    vec3 diffuse;  ///< 漫反射光的 RGB 强度。
    vec3 specular; ///< 镜面反射光的 RGB 强度。
};

varying vec3 fragPos;
varying vec3 Normal;

uniform vec3 viewPos;
uniform Material material;
uniform Light light1;
uniform Light light2;

varying vec3 mater;

vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;
    return (ambient + diffuse + specular);
}

void main()
{

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 result = CalcDirLight(light1, norm, viewDir);
    gl_FragColor = vec4(mater * result, 1.0);
    // gl_FragColor = vec4(mater*vec3(0.7,0.8,0.7), 1.0);
}
