[27;5;95~/
//    Relief Mapping for FX Composer
// 
//    Original code written by Fabio Policarpo.
//    Manuel M. Oliveira fixed and documented the vertex shader, 
//    and added refraction, chromatic dispersion, Fresnel effect,
//    and self-shadowing with Chromatic Dispersion and Fresnel. 
//    
//    Last modified on October 2, 2005. 
//
//    Copyright (c) 2005, Fabio Policarpo and Manuel M. Oliveira
//

string description = "Relief Mapping with refraction, chromatic dispersion and Fresnel effect by Manuel M. Oliveira and Fabio Policarpo";

texture theEnvironmentCube
<
    string ResourceName = "default_reflection.dds";
    string TextureType = "CUBE";
>;

samplerCUBE environmentTexture_sampler = sampler_state
{
   Texture = <theEnvironmentCube>;
   MinFilter = Linear;
   MagFilter = Linear;
   MipFilter = Linear;
   AddressU = Clamp;
   AddressV = Clamp;
   AddressW = Clamp;
};

float3 etaRatios
<
  string UIName = "Refraction indices";
> = {1.50, 1.54, 1.58};

float fresnelBias
<
  string UIWidget = "slider";
  float UIMin = 0.0;
  float UImax = 2.0;
  float UIStep = 0.02;
  string UIName = "Fresnel Bias";
> = 0.36;

float fresnelScale
<
  string UIWidget = "slider";
  float UIMin = 0.0;
  float UImax = 8.0;
  float UIStep = 0.02;
  string UIName = "Fresnel Scale";
> = 1.0;

float fresnelPower
<
  string UIWidget = "slider";
  float UIMin = 0.0;
  float UImax = 20.0;
  float UIStep = 0.5;
  string UIName = "Fresnel Power";
> = 5.0;

float tile
<
	string UIName = "Tile Factor";
	string UIWidget = "slider";
	float UIMin = 1.0;
	float UIStep = 1.0;
	float UIMax = 32.0;
> = 8;

float depth
<
	string UIName = "Depth Factor";
	string UIWidget = "slider";
	float UIMin = 0.0f;
	float UIStep = 0.01f;
	float UIMax = 0.25f;
> = 0.1;

float3 ambient
<
	string UIName = "Ambient";
	string UIWidget = "color";
//> = {0.05,0.05,0.05};
> = {0.2,0.2,0.2};

float3 diffuse
<
	string UIName = "Diffuse";
	string UIWidget = "color";
> = {1,1,1};

float3 specular
<
	string UIName = "Specular";
	string UIWidget = "color";
> = {0.75,0.75,0.75};

float shine
<
    string UIName = "Shine";
	string UIWidget = "slider";
	float UIMin = 8.0f;
	float UIStep = 8;
	float UIMax = 256.0f;
> = 128.0;

float3 lightpos : POSITION
<
	string UIName="Light Position";
> = { 100,100,0 };

float4x4 modelviewproj : WorldViewProjection;
float4x4 modelview     : WorldView;
float4x4 modelviewIT   : WorldViewInverseTranspose;
float4x4 modelinv      : WorldInverse;
float4x4 view : View;

texture texmap : DIFFUSE
<
    string ResourceName = "rockwall.jpg";
    string ResourceType = "2D";
>;

texture reliefmap : NORMAL
<
    string ResourceName = "rockwall.tga";
    string ResourceType = "2D";
>;

sampler2D texmap_sampler = sampler_state
{
   Texture = <texmap>;
   MinFilter = Linear;
   MagFilter = Linear;
   MipFilter = Linear;
};

sampler2D reliefmap_sampler = sampler_state
{
   Texture = <reliefmap>;
   MinFilter = Linear;
   MagFilter = Linear;
   MipFilter = Linear;
};

struct a2v 
{
   // vertex attributes in object space
   float4 pos : POSITION;
   float3 normal : NORMAL;
   float2 texcoord : TEXCOORD0;
   float3 tangent : TANGENT0;
   float3 binormal : BINORMAL0;
};

struct v2f
{
   float4 hpos : POSITION;
   float3 eye : TEXCOORD0;
   float3 light : TEXCOORD1;
   float2 texcoord : TEXCOORD2;
};

v2f view_space(a2v IN)
{
   v2f OUT;

   // vertex position in clip space
   OUT.hpos = mul(IN.pos, modelviewproj);

   // copy color and texture coordinates
   OUT.texcoord = IN.texcoord.xy * tile;

   // tangent vectors in view space
   float3 tangent = mul(IN.tangent, modelviewIT).xyz;
   float3 binormal = mul(IN.binormal, modelviewIT).xyz;
   float3 normal = mul(IN.normal, modelviewIT).xyz;
   //
   //  create a transformation matrix with each vector as a row
   //
   float3x3 tangentspace = float3x3(tangent, binormal, normal);

   // vertex position in view space (with model transformations)
   float3 vpos = mul(IN.pos, modelview).xyz;

   // view in tangent space
   //
   //  the matrix required to project a column vector to tangent space
   //  is given by:
   //                    | Tx  Ty  Tz |   |Vx|
   //                    | Bx  By  Bz | x |Vy|
   //                    | Nx  Ny  Nz |   |Vz|
   //
   float3 eye = mul(tangentspace, vpos);
   eye.z = -eye.z;
   OUT.eye = eye;

   // light position in tangent space
   float4 light = float4(lightpos.x, lightpos.y, lightpos.z, 1);
   light = mul(light, view);
   light.xyz = mul(tangentspace, light.xyz - vpos);
   OUT.light = light.xyz;

   return OUT;
}

float4 normal_map(
	v2f IN,
	uniform sampler2D texmap,
	uniform sampler2D reliefmap) : COLOR
{
   float2 uv = IN.texcoord;

   // normal map
   float4 normal = tex2D(reliefmap, uv);
   normal.xyz = normalize(normal.xyz - 0.5);
   normal.y = -normal.y;

   // color map
   float4 color = tex2D(texmap, uv);

   // view and light directions
   float3 v = normalize(IN.eye);
   float3 l = normalize(IN.light);

   // compute diffuse and specular terms
   float diff = saturate(dot(l, normal.xyz));
   float spec = saturate(dot(normalize(l - v), normal.xyz));

   // compute final color
   float4 finalcolor;
   finalcolor.xyz = ambient * color.xyz + diff * (color.xyz * diffuse + specular * pow(spec, shine));
   finalcolor.w = 1.0;

   return finalcolor;
}

float4 parallax_map(
	v2f IN,
	uniform sampler2D texmap,
	uniform sampler2D reliefmap) : COLOR
{
   // view and light directions
   float3 v = normalize(IN.eye);
   float3 l = normalize(IN.light);

   float2 uv = IN.texcoord;

   // parallax code
   float height = tex2D(reliefmap, uv).w * 0.06 - 0.03;
   uv += height * v.xy;

   // normal map
   float4 normal = tex2D(reliefmap, uv);
   normal.xyz = normalize(normal.xyz - 0.5);
   normal.y = -normal.y;

   // color map
   float4 color = tex2D(texmap, uv);

   // compute diffuse and specular terms
   float diff = saturate(dot(l, normal.xyz));
   float spec = saturate(dot(normalize(l - v), normal.xyz));

   // compute final color
   float4 finalcolor;
   finalcolor.xyz = ambient * color.xyz + diff * (color.xyz * diffuse + specular * pow(spec, shine));
   finalcolor.w = 1.0;

   return finalcolor;
}

// use linear and binary search
float3 ray_intersect_rm(
      in sampler2D reliefmap,
      in float3 s, 
      in float3 ds)
{
   const int linear_search_steps = 10;
   const int binary_search_steps = 5;

   ds /= linear_search_steps;

   for (int i = 0; i < linear_search_steps - 1; i++) {
      float4 t = tex2D(reliefmap, s);

      if (s.z < t.w)
         s += ds;
   }

   for (int i = 0; i < binary_search_steps; i++) {
      ds *= 0.5;
      float4 t = tex2D(reliefmap, s);
      if (s.z < t.w)
         s += 2 * ds;
      s -= ds;
   }

   return s;
}

// only linear search for shadows
float3 ray_intersect_rm_lin(
      in sampler2D reliefmap,
      in float3 s, 
      in float3 ds)
{
   const int linear_search_steps = 10;
   ds /= linear_search_steps;

   for (int i = 0; i < linear_search_steps - 1; i++) {
      float4 t = tex2D(reliefmap, s);
      if (s.z < t.w)
         s += ds;
   }

   return s;
}

float4 relief_map_shadows(
	v2f IN,
	uniform sampler2D texmap,
	uniform sampler2D reliefmap) : COLOR
{
   // view direction
   float3 v = normalize(IN.eye);

   // serach start point and serach vector with depth bias
   float3 s = float3(IN.texcoord, 0);
   v.xy *= depth * (2 * v.z - v.z * v.z);
   v /= v.z;

   // ray intersect depth map
   float3 tx = ray_intersect_rm(reliefmap, s, v);

   // displace start position to intersetcion point
   s.xy += v.xy * tx.z;

   // get normal and color at intersection point
   float4 n = tex2D(reliefmap, tx.xy);
   float4 c = tex2D(texmap, tx.xy);

   // expand normal
   n.xyz = n.xyz * 2 - 1;
   n.y = -n.y;

   // light direction
   float3 l = normalize(IN.light);

   // view direction
   v = normalize(IN.eye);
   v.z = -v.z;

   // compute diffuse and specular terms
   float diff = saturate(dot(l, n.xyz));
   float spec = saturate(dot(normalize(l - v), n.xyz));

   // light serch vector with depth bias
   l.xy *= depth * (2 * l.z - l.z * l.z);
   l.z = -l.z;
   l /= l.z;

   // displace start position to light entry point
   s.xy -= l.xy * tx.z;

   // ray intersect from light
   float3 tx2 = ray_intersect_rm_lin(reliefmap, s, l);

   if (tx2.z < tx.z - 0.05) // if pixel in shadow
   {
      diff *= dot(ambient.xyz, float3(1.0, 1.0, 1.0)) * 0.333333;
      spec = 0;
   }

   // compute final color
   float4 finalcolor;
   finalcolor.xyz = ambient * c.xyz + diff * (c.xyz * diffuse + specular * pow(spec, shine));
   finalcolor.w = 1.0;

   return finalcolor;
}

float4 relief_map(
	v2f IN,
	uniform sampler2D texmap,
	uniform sampler2D reliefmap) : COLOR
{
   // ray intersect in view direction
   float3 v = normalize(IN.eye);

   // serach start point and serach vector with depth bias
   float3 s = float3(IN.texcoord, 0);
   v.xy *= depth * (2 * v.z - v.z * v.z);
   v /= v.z;

   // ray intersect depth map
   float3 tx = ray_intersect_rm(reliefmap, s, v);

   // get normal and color at intersection point
   float4 n = tex2D(reliefmap, tx.xy);
   float4 c = tex2D(texmap, tx.xy);

   // expand normal
   n.xyz = n.xyz * 2 - 1;
   n.y = -n.y;

   // light direction
   float3 l = normalize(IN.light);

   // view direction
   v = normalize(IN.eye);
   v.z = -v.z;

   float4 finalcolor;

   // compute diffuse and specular terms
   float diff = saturate(dot(l, n.xyz));
   float spec = saturate(dot(normalize(l - v), n.xyz));

   // compute final color
   finalcolor.xyz = ambient * c.xyz + diff * (c.xyz * diffuse + specular * pow(spec, shine));
   finalcolor.w = c.w;

   return finalcolor;
}

//
//  Relief Mapping with refraction, chromatic dispersion and Fresnel effect
//  by Manuel Menezes de Oliveira Neto
//
float4 relief_map_Fresnel(
	v2f IN,
	uniform sampler2D texmap,
	uniform sampler2D reliefmap,
	uniform samplerCUBE environmentTexture_sampler) : COLOR
{
   // ray intersect in view direction
   float3 v = normalize(IN.eye);

   // serach start point and search vector with depth bias
   float3 s = float3(IN.texcoord, 0);
   v.xy *= depth * (2 * v.z - v.z * v.z);
   v /= v.z;

   // ray intersect depth map
   float3 tx = ray_intersect_rm(reliefmap, s, v);

   // get normal and color at intersection point
   float4 n = tex2D(reliefmap, tx.xy);
   float4 c = tex2D(texmap, tx.xy);

   // expand normal
   n.xyz = n.xyz * 2 - 1;
   n.y = -n.y;

   // light direction
   float3 l = normalize(IN.light);

   // view direction
   v = normalize(IN.eye);
   v.z = -v.z;

   float4 finalcolor;

   //
   //   computes the directions of the refracted rays
   //
   float3 RefDir_Red = refract(v, n.xyz, etaRatios.r);
   float3 RefDir_Green = refract(v, n.xyz, etaRatios.g);
   float3 RefDir_Blue = refract(v, n.xyz, etaRatios.b);
   //
   //   compute the color components of the refracted rays
   //   resulting from the chromatic dispersion
   //
   float4 RefractColor;
   RefractColor.r = texCUBE(environmentTexture_sampler, RefDir_Red).r;
   RefractColor.g = texCUBE(environmentTexture_sampler, RefDir_Green).g;
   RefractColor.b = texCUBE(environmentTexture_sampler, RefDir_Blue).b;
   RefractColor.a = 1.0;
   //
   //   compute the reflected direction
   //
   float3 ReflectDir = reflect(v, n.xyz);
   //
   //   compute the reflected color
   //
   float4 ReflectColor = texCUBE(environmentTexture_sampler, ReflectDir);

   float refl_factor = fresnelBias + fresnelScale * pow(1.0 + dot(v, n.xyz), fresnelPower);

   finalcolor = lerp(RefractColor, ReflectColor, refl_factor);

   return finalcolor;
}

//
//  Relief Mapping with shadows, refraction, chromatic dispersion,
//  Fresnel effect, and Shadows
//  by Manuel Menezes de Oliveira Neto
//
float4 relief_map_Fresnel_shadows(
	v2f IN,
	uniform sampler2D texmap,
	uniform sampler2D reliefmap,
	uniform samplerCUBE environmentTexture_sampler) : COLOR
{
   // view direction
   float3 v = normalize(IN.eye);

   // serach start point and serach vector with depth bias
   float3 s = float3(IN.texcoord, 0);
   v.xy *= depth * (2 * v.z - v.z * v.z);
   v /= v.z;

   // ray intersect depth map
   float3 tx = ray_intersect_rm(reliefmap, s, v);

   // displace start position to intersetcion point
   s.xy += v.xy * tx.z;

   // get normal and color at intersection point
   float4 n = tex2D(reliefmap, tx.xy);
   float4 c = tex2D(texmap, tx.xy);

   // expand normal
   n.xyz = n.xyz * 2 - 1;
   n.y = -n.y;

   // light direction
   float3 l = normalize(IN.light);

   // view direction
   v = normalize(IN.eye);
   v.z = -v.z;

   // compute diffuse and specular terms
   float diff = saturate(dot(l, n.xyz));
   float spec = saturate(dot(normalize(l - v), n.xyz));

   // light serch vector with depth bias
   l.xy *= depth * (2 * l.z - l.z * l.z);
   l.z = -l.z;
   l /= l.z;

   // displace start position to light entry point
   s.xy -= l.xy * tx.z;

   // ray intersect from light
   float3 tx2 = ray_intersect_rm_lin(reliefmap, s, l);

   if (tx2.z < tx.z - 0.05) // if pixel in shadow
   {
      diff *= dot(ambient.xyz, float3(1.0, 1.0, 1.0)) * 0.333333;
      spec = 0;
   }
   //
   //   computes the directions of the refracted rays
   //
   float3 RefDir_Red = refract(v, n.xyz, etaRatios.r);
   float3 RefDir_Green = refract(v, n.xyz, etaRatios.g);
   float3 RefDir_Blue = refract(v, n.xyz, etaRatios.b);
   //
   //   compute the color components of the refracted rays
   //   resulting from the chromatic dispersion
   //
   float4 RefractColor;
   RefractColor.r = texCUBE(environmentTexture_sampler, RefDir_Red).r;
   RefractColor.g = texCUBE(environmentTexture_sampler, RefDir_Green).g;
   RefractColor.b = texCUBE(environmentTexture_sampler, RefDir_Blue).b;
   RefractColor.a = 1.0;
   //
   //   compute the reflected direction
   //
   float3 ReflectDir = reflect(v, n.xyz);
   //
   //   compute the reflected color
   //
   float4 ReflectColor = texCUBE(environmentTexture_sampler, ReflectDir);

   float refl_factor = fresnelBias + fresnelScale * pow(1.0 + dot(v, n.xyz), fresnelPower);

   float4 interm_color = lerp(RefractColor, ReflectColor, refl_factor);

   // compute final color
   float4 finalcolor;
   finalcolor.xyz = ambient * interm_color.xyz + diff * (interm_color.xyz + specular * pow(spec, shine));
   finalcolor.w = 1.0;

   return finalcolor;
}


technique normal_mapping
{
   pass p0 {
      CullMode = CCW;
      VertexShader = compile vs_1_1 view_space();
      PixelShader = compile ps_2_0 normal_map(texmap_sampler, reliefmap_sampler);
   }
}

technique parallax_mapping
{
   pass p0 {
      CullMode = CCW;
      VertexShader = compile vs_1_1 view_space();
      PixelShader = compile ps_2_0 parallax_map(texmap_sampler, reliefmap_sampler);
   }
}

technique relief_mapping
{
   pass p0 {
      CullMode = CCW;
      VertexShader = compile vs_1_1 view_space();
      PixelShader = compile ps_2_a relief_map(texmap_sampler, reliefmap_sampler);
   }
}

technique relief_mapping_shadows
{
   pass p0 {
      CullMode = CCW;
      VertexShader = compile vs_1_1 view_space();
      PixelShader = compile ps_2_a relief_map_shadows(texmap_sampler, reliefmap_sampler);
   }
}

technique relief_mapping_Fresnel
{
   pass p0 {
      CullMode = CCW;
      VertexShader = compile vs_1_1 view_space();
      PixelShader = compile ps_2_a relief_map_Fresnel(texmap_sampler, reliefmap_sampler, environmentTexture_sampler);
   }
}

technique relief_mapping_Fresnel_shadows
{
   pass p0 {
      CullMode = CCW;
      VertexShader = compile vs_1_1 view_space();
      PixelShader = compile ps_2_a relief_map_Fresnel_shadows(texmap_sampler, reliefmap_sampler, environmentTexture_sampler);
   }
}
