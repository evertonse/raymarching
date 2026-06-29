Shader "quad_depth_relief_mapping" 
{
	Properties 
	{
		depth 	("Depth Factor", Float) = 0.1
		tile 	("Tile Factor", Float) = 1
		
		ambient_color 	("Ambient",  Color) = (0.2, 0.2, 0.2)
		diffuse_color 	("Diffuse",  Color) = (1, 1, 1)
		specular_color 	("Specular", Color) = (0.75,0.75,0.75)
		
		shine 	("Shine", Float) = 128
		
		lightpos ("Light Position (view space)", Vector) = (100.0, -50.0, 50.0)
		
		color_map 		("Color Map"	 ,2D) = "white" {}
		quad_depth_map 	("Quad Depth Map",2D) = "white" {}
		normal_map_x	("Normal Map X"	 ,2D) = "white" {}
		normal_map_y 	("Normal Map Y"	 ,2D) = "white" {}

	}
	SubShader 
	{
		Pass 
		{		
		Cull off

CGPROGRAM //-----------

#pragma target 3.0	
#pragma vertex 	 view_space
#pragma fragment relief_map_quad_depth
#pragma profileoption MaxTexIndirections=64

//----- uniform
float 	depth;
float 	tile;
float3 	ambient_color;
float3 	diffuse_color;
float3 	specular_color;
float 	shine;
float3 	lightpos : POSITION;

//----- application data		

struct a2v 
{
    float4 vertex       : POSITION;		// float4 pos
    float3 normal    	: NORMAL;
    float2 texcoord  	: TEXCOORD0;
    float4 tangent   	: TANGENT0;		// float3 tangent
    //float3 binormal   : BINORMAL0;	// Not found in Unity3d
};

struct v2f
{
	float4 hpos 	: POSITION;
	float3 eye 		: TEXCOORD0;
	float3 light 	: TEXCOORD1;
	float2 texcoord : TEXCOORD2;
};

//----- vetrex shader
v2f view_space(a2v IN) 
{ 

	v2f OUT;

	// vertex position in object space
	float4 pos=float4(IN.vertex.x,IN.vertex.y,IN.vertex.z,1.0);

	// vertex position IN clip space
	OUT.hpos 		= mul(glstate.matrix.mvp, pos);

	// copy color and texture coordinates
	OUT.texcoord=IN.texcoord.xy*tile;

	// compute modelview rotation only part
	float3x3 modelviewrot	=float3x3(glstate.matrix.modelview[0]);

	// tangent vectors IN view space	
	float3 IN_bINormal 		= cross( IN.normal, IN.tangent.xyz )*IN.tangent.w;	 
	float3 tangent			= mul(modelviewrot,	IN.tangent.xyz);
	float3 bINormal			= mul(modelviewrot,	IN_bINormal.xyz);	// IN.biNormal
	float3 normal			= mul(modelviewrot,	IN.normal);
	float3x3 tangentspace	= float3x3(tangent,	bINormal,normal);

	// vertex position IN view space (with model transformations)
	float3 vpos=mul(glstate.matrix.modelview[0],pos).xyz;

	// view in tangent space
	float3 eye=mul(tangentspace,vpos);
	eye.z=-eye.z;
	OUT.eye=eye;
	
	// light position in tangent space
	OUT.light=mul(tangentspace,lightpos -vpos);
	//OUT.light=mul(tangentspace,glstate.light[0].position.xyz -vpos);

	return OUT;
}
		
		
// ray intersect quad depth map with linear search
void ray_intersect_rmqd_linear(
      in sampler2D quad_depth_map,
      inout float3 s,
      inout float3 ds)
{
   const int linear_search_steps=30;  //15
   
   ds/=linear_search_steps;
   
   // search front to back for first point inside object
   for( int i=0;i<linear_search_steps-1;i++ )
   {
		float4 t=tex2D(quad_depth_map,s.xy);
		
		float4 d=s.z-t;									// compute distances to each layer
		d.xy*=d.zw;	d.x*=d.y;							// x=(x*y)*(z*w)
		
		if (d.x>0)										// if ouside object move forward
			s+=ds;
   }
}

// ray intersect quad depth map with binary search
void ray_intersect_rmqd_binary(
      in sampler2D quad_depth_map,
      inout float3 s, 
      inout float3 ds)
{
   const int binary_search_steps=5;
   
   float3 ss=sign(ds.z);

   // recurse around first point for closest match
   for( int i=0;i<binary_search_steps;i++ )
   {
		ds*=0.5;										// half size at each step
		
		float4 t=tex2D(quad_depth_map,s.xy);
		
		float4 d=s.z-t;									// compute distances to each layer
		d.xy*=d.zw;	d.x*=d.y;							// x=(x*y)*(z*w)
		
		if (d.x<0)										// if inside
		{
			ss=s;										// store good return position
			s-=2*ds;									// move backward
		}
		s+=ds;											// else move forward
   }
   
   s=ss;
}

float4 relief_map_quad_depth(
								v2f IN,
								uniform sampler2D quad_depth_map,
								uniform sampler2D color_map,
								uniform sampler2D normal_map_x,
								uniform sampler2D normal_map_y	) : COLOR
{
	// view vector in tangent space
	float3 v=normalize(IN.eye);

	// serach start position
	float3 s=float3(IN.texcoord,0);
	
	// separate direction (front or back face)
	float dir=v.z;
	v.z=abs(v.z);
	
	// depth bias (1-(1-d)*(1-d))
	float d=depth*(2*v.z-v.z*v.z);

	// compute serach vector
	v/=v.z;
	v.xy*=d;
	s.xy-=v.xy*0.5;
	
	// if viewing from backface
	if (dir<0)
	{	
		s.z=0.996;	// search from back to front
		v.z=-v.z;
	}
	
	// ray intersect quad depth map
	ray_intersect_rmqd_linear(quad_depth_map,s,v);
	ray_intersect_rmqd_binary(quad_depth_map,s,v);
	
	// discard if no intersection is found
	if (s.z>0.997) discard;
	if (s.z<0.003) discard;

	// DEBUG: return depth
	//return float4(s.zzz,1);

	// get quad depth and color at intersection
	float4 t=tex2D(quad_depth_map,s.xy);
	float4 c=tex2D(color_map,s.xy);

	// get normal components X and Y
	float4 nx=tex2D(normal_map_x,s.xy);
	float4 ny=tex2D(normal_map_y,s.xy);
	
	// compute normal
	float4 z=abs(s.z-t);
	int m=0;											// find min component
	float zm = z.x;
	if (z.y<zm) { m=1; zm = z.y; }
	if (z.z<zm) { m=2; zm = z.z; }
	if (z.w<zm) { m=3; 			 }
	
	float3 n; 	 										// get normal at min component layer	
	if ( m == 0) { n.x=nx[0]; n.y=1-ny[0]; }			//n.x=nx[m]; n.y=1-ny[m];	
	if ( m == 1) { n.x=nx[1]; n.y=1-ny[1]; }
	if ( m == 2) { n.x=nx[2]; n.y=1-ny[2]; }
	if ( m == 3) { n.x=nx[3]; n.y=1-ny[3]; }
	 
	n.xy=n.xy*2-1; 										// expand to [-1,1] range
	n.z=sqrt(max(0,1.0-dot(n.xy,n.xy))); 				// recompute z
	if (m==1||m==3) 									// invert normal z if in backface
		n.z=-n.z;

	// DEBUG: return normal
	// return float4(n*0.5+0.5,1);

	// compute light vector in view space
	float3 l=normalize(IN.light);

	// restore view direction z component
	v=normalize(IN.eye);
	v.z=-v.z;
	
	// compute diffuse and specular terms
	float ldotn=saturate(dot(l,n));
	float ndoth=saturate(dot(n,normalize(l-v)));
	
	// attenuation factor
	float att=1.0-max(0,l.z); att=1.0-att*att;

	// return final color with lighting
	float4 finalcolor;
	finalcolor.xyz = 	c.xyz*ambient_color + 
						att*(c.xyz*diffuse_color*ldotn +
						c.w*specular_color.xyz*pow(ndoth,shine));
	finalcolor.w=1;
	
	return finalcolor;
}
		
ENDCG //----------		
		} // Pass
	} // SubShader
} // Shader

float3 ambient
<
	string UIName = "Ambient";
	string UIWidget = "color";
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
> = 32.0;

float tile
<
	string UIName = "Tile Factor";
	string UIWidget = "slider";
	float UIMin = 1.0;
	float UIStep = 1.0;
	float UIMax = 32.0;
> = 1;

float depth
<
	string UIName = "Depth Factor";
	string UIWidget = "slider";
	float UIMin = 0.01f;
	float UIStep = 0.01f;
	float UIMax = 0.50f;
> = 0.1;

bool depth_bias
<
	string UIName = "Depth Bias";
	string UIWidget = "checkbox";
> = false;

bool border_clamp
<
	string UIName = "Border Clamp";
	string UIWidget = "checkbox";
> = false;

texture color_tex : DIFFUSE
<
    string ResourceName = "tile1.jpg";
    string ResourceType = "2D";
>;

texture relaxedcone_relief_tex : DIFFUSE
<
    string ResourceName = "tile1_relaxedcone.tga";
    string ResourceType = "2D";
>;

texture cone_relief_tex : DIFFUSE
<
    string ResourceName = "tile1_cone.tga";
    string ResourceType = "2D";
>;

texture quadcone_relief_tex : DIFFUSE
<
    string ResourceName = "tile1_quadcone.tga";
    string ResourceType = "2D";
>;

sampler2D color_map = sampler_state
{
	Texture = <color_tex>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
};

sampler2D relaxedcone_relief_map = sampler_state
{
	Texture = <relaxedcone_relief_tex>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = None;
};

sampler2D cone_relief_map = sampler_state
{
	Texture = <cone_relief_tex>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = None;
};

sampler2D quadcone_relief_map = sampler_state
{
	Texture = <quadcone_relief_tex>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = None;
};

float3 lightpos : Position 
<
    string Object = "PointLight";
    string Space = "World";
    string UIWidget = "none";
> = {-10.0f, 10.0f, -10.0f};

float4x4 modelviewproj_matrix : WorldViewProjection <string UIWidget = "none";>;
float4x4 modelview_matrix : WorldView <string UIWidget = "none";>;
float4x4 view_matrix : View <string UIWidget = "none";>;

struct a2v 
{
    float4 pos       : POSITION;
    float3 normal    : NORMAL;
    float2 texcoord  : TEXCOORD0;
    float3 tangent   : TANGENT0;
    float3 binormal  : BINORMAL0;
};

struct v2f
{
	float4 hpos : POSITION;
	float3 eye : TEXCOORD0;
	float3 light : TEXCOORD1;
	float2 texcoord : TEXCOORD2;
};

v2f vertex_shader(a2v IN)
{
	v2f OUT;

	// vertex position in object space
	float4 pos=float4(IN.pos.x,IN.pos.y,IN.pos.z,1.0);

	// vertex position in clip space
	OUT.hpos=mul(pos,modelviewproj_matrix);

	// copy color and texture coordinates
	OUT.texcoord=IN.texcoord.xy*tile;

	// compute modelview rotation only part
	float3x3 modelviewrot;
	modelviewrot[0]=modelview_matrix[0].xyz;
	modelviewrot[1]=modelview_matrix[1].xyz;
	modelviewrot[2]=modelview_matrix[2].xyz;

	// tangent vectors in view space
	float3 tangent=mul(IN.tangent,modelviewrot);
	float3 binormal=mul(IN.binormal,modelviewrot);
	float3 normal=mul(IN.normal,modelviewrot);
	float3x3 tangentspace=float3x3(tangent,binormal,normal);

	// vertex position in view space (with model transformations)
	float3 vpos=mul(pos,modelview_matrix).xyz;

	// view in tangent space
	OUT.eye=mul(tangentspace,vpos);
	
	// light position in tangent space
	float4 light=float4(lightpos.x,lightpos.y,lightpos.z,1);
	light=mul(light,view_matrix);
	light.xyz=mul(tangentspace,light.xyz-vpos);
	OUT.light=light.xyz;

	return OUT;
}

// setup ray pos and dir based on view vector
// and apply depth bias and depth factor
void setup_ray(v2f IN,out float3 p,out float3 v)
{
	p = float3(IN.texcoord,0);
	v = normalize(IN.eye);
	
	v.z = abs(v.z);

	if (depth_bias)
	{
		float db = 1.0-v.z; db*=db; db*=db; db=1.0-db*db;
		v.xy *= db;
	}
	
	v.xy *= depth;
}

// do normal mapping using given texture coordinate
// tangent space phong lighting with optional border clamp
// normal X and Y stored in red and green channels
float4 normal_mapping(
	sampler2D color_map,
	sampler2D normal_map,
	float2 texcoord,
	v2f IN)
{
	// color map
	float4 color = tex2D(color_map,texcoord);
	
	// normal map
	float4 normal = tex2D(normal_map,texcoord);
	normal.xy = 2*normal.xy - 1;
	normal.y = -normal.y;
	normal.z = sqrt(1.0 - dot(normal.xy,normal.xy));

	// light and view in tangent space
	float3 l = normalize(IN.light);
	float3 v = normalize(IN.eye);

	// compute diffuse and specular terms
	float diff = saturate(dot(l,normal.xyz));
	float spec = saturate(dot(normalize(l-v),normal.xyz));

	// attenuation factor
	float att = 1.0 - max(0,l.z); 
	att = 1.0 - att*att;

	// border clamp
	float alpha=1;
	if (border_clamp)
	{
		if (texcoord.x<0) alpha=0;
		if (texcoord.y<0) alpha=0;
		if (texcoord.x>tile) alpha=0;
		if (texcoord.y>tile) alpha=0;
	}
	
	// compute final color
	float4 finalcolor;
	finalcolor.xyz = ambient*color.xyz +
		att*(color.xyz*diffuse*diff +
		specular*pow(spec,shine));
	finalcolor.w = alpha;
	return finalcolor;
}

// ray intersect depth map using binary cone space leaping
// depth value stored in alpha channel (black is at object surface)
// and cone ratio stored in blue channel
void ray_intersect_relaxedcone(sampler2D relaxedcone_relief_map, inout float3 p, inout float3 v) {
   // const int cone_steps=15;
   // const int binary_steps=8;
   const int cone_steps   = 32;
   const int binary_steps = 16;

   float3 p0 = p;

   v /= v.z;

   float dist = length(v.xy);

   for (int i = 0; i < cone_steps; i++) {
      float4 tex = tex2D(relaxedcone_relief_map, p.xy);

      float height = saturate(tex.w - p.z);

      float cone_ratio = tex.z;

      p += v * (cone_ratio * height / (dist + cone_ratio));
   }

   v *= p.z * 0.5;
   p = p0 + v;

   for (int i = 0; i < binary_steps; i++) {
      float4 tex = tex2D(relaxedcone_relief_map, p.xy);
      v *= 0.5;
      if (p.z < tex.w)
         p += v;
      else
         p -= v;
   }
}

// ray intersect depth map using quad cone space leaping
// depth value stored in alpha channel (black is at object surface)
// and RGBA texture with pyramid ratios for each main direction (N,S,W,E)
void ray_intersect_quadcone(
	sampler2D cone_relief_map,
	sampler2D quadcone_relief_map,
	inout float3 p,
	inout float3 v)
{
	const int num_steps=15;
	
	float2 abs_view = abs(v.xy);
	int index;
	if (abs_view.x>abs_view.y)
		index = v.x<0 ? 1 : 0;
	else
		index = v.y<0 ? 3 : 2;
	
	float dist = length(v.xy);

	for( int i=0;i<num_steps;i++ )
	{
		float4 tex = tex2D(cone_relief_map, p.xy);

		float height = saturate(tex.w - p.z);
		
		float cone_ratio = tex2D(quadcone_relief_map, p.xy)[index];
		
		p += v * (cone_ratio * height / (dist + cone_ratio));
	}
}

// ray intersect depth map using cone space leaping
// depth value stored in alpha channel (black is at object surface)
// and cone ratio stored in blue channel
void ray_intersect_cone(
	sampler2D cone_relief_map,
	inout float3 p,
	inout float3 v)
{
	const int num_steps = 15;
	
	float dist = length(v.xy);
	
	for( int i=0;i<num_steps;i++ )
	{
		float4 tex = tex2D(cone_relief_map, p.xy);
		
		float height = saturate(tex.w - p.z);
		
		float cone_ratio = tex.z;
		
		p += v * (cone_ratio * height / (dist + cone_ratio));
	}
}

// ray intersect depth map using linear and binary searches
// depth value stored in alpha channel (black at is object surface)
void ray_intersect_relief(
	sampler2D relief_map,
	inout float3 p,
	inout float3 v)
{
	const int num_steps_lin=15;
	const int num_steps_bin=6;
	
	v /= v.z*num_steps_lin;
	
	int i;
	for( i=0;i<num_steps_lin;i++ )
	{
		float4 tex = tex2D(relief_map, p.xy);
		if (p.z<tex.w)
			p+=v;
	}
	
	for( i=0;i<num_steps_bin;i++ )
	{
		v *= 0.5;
		float4 tex = tex2D(relief_map, p.xy);
		if (p.z<tex.w)
			p+=v;
		else
			p-=v;
	}
}

float4 pixel_shader_relaxedcone(v2f IN) : COLOR
{
	float3 p,v;
	
	setup_ray(IN,p,v);

	ray_intersect_relaxedcone(relaxedcone_relief_map,p,v);

	return normal_mapping(color_map,relaxedcone_relief_map,p.xy,IN);
}

float4 pixel_shader_quadcone(v2f IN) : COLOR
{
	float3 p,v;
	
	setup_ray(IN,p,v);

	ray_intersect_quadcone(cone_relief_map,quadcone_relief_map,p,v);

	return normal_mapping(color_map,cone_relief_map,p.xy,IN);
}

float4 pixel_shader_cone(v2f IN) : COLOR
{
	float3 p,v;
	
	setup_ray(IN,p,v);

	ray_intersect_cone(cone_relief_map,p,v);

	return normal_mapping(color_map,cone_relief_map,p.xy,IN);
}

float4 pixel_shader_relief(v2f IN) : COLOR
{
	float3 p,v;
	
	setup_ray(IN,p,v);

	ray_intersect_relief(cone_relief_map,p.xyz,v);
	
	return normal_mapping(color_map,cone_relief_map,p.xy,IN);
}

float4 pixel_shader_normal(v2f IN) : COLOR
{
	return normal_mapping(color_map,cone_relief_map,IN.texcoord,IN);
}

technique relaxed_cone_mapping
{
    pass p0 
    {
		CullMode = CCW;
   		AlphaTestEnable = True;
		AlphaFunc = GreaterEqual;
		AlphaRef = 16;
    	
		VertexShader = compile vs_1_1 vertex_shader();
		PixelShader  = compile ps_2_a pixel_shader_relaxedcone();
    }
}

technique quad_cone_mapping
{
    pass p0 
    {
		CullMode = CCW;
   		AlphaTestEnable = True;
		AlphaFunc = GreaterEqual;
		AlphaRef = 16;
    	
		VertexShader = compile vs_1_1 vertex_shader();
		PixelShader  = compile ps_2_a pixel_shader_quadcone();
    }
}

technique cone_mapping
{
    pass p0 
    {
		CullMode = CCW;
   		AlphaTestEnable = True;
		AlphaFunc = GreaterEqual;
		AlphaRef = 16;
    	
		VertexShader = compile vs_1_1 vertex_shader();
		PixelShader  = compile ps_2_a pixel_shader_cone();
    }
}

technique relief_mapping
{
    pass p0 
    {
		CullMode = CCW;
   		AlphaTestEnable = True;
		AlphaFunc = GreaterEqual;
		AlphaRef = 16;
		
		VertexShader = compile vs_1_1 vertex_shader();
		PixelShader  = compile ps_2_a pixel_shader_relief();
    }
}

technique normal_mapping
{
    pass p0 
    {
		CullMode = CCW;
    	
		VertexShader = compile vs_1_1 vertex_shader();
		PixelShader  = compile ps_2_0 pixel_shader_normal();
    }
}

// Adapted from "Cone Step Mapping: An Iterative Ray-Heightfield Intersection Algorithm" - Jonathan Dummer
HMapIntersection findIntersection_coneStepMapping(float2 u, float2 u2)
{
    float3 ds = float3(u2 - u, 1);
    ds = normalize(ds);
    float w = 1 / HMres.x;
    float iz = sqrt(1.0 - ds.z * ds.z); // = length(ds.xy)
    float sc = 0;
    float2 t = getHC_texture(u);
    int stepCount = 0;
    float zTimesSc = 0.0;
    while (1.0 - ds.z * sc > t.x && stepCount < steps)
    {
        zTimesSc = ds.z * sc;
        sc += relax * (w + (1.0 - zTimesSc - t.x) / (ds.z + iz / ( /*t.y **/t.y)));
        t = getHC_texture(u + ds.xy * sc);
        ++stepCount;
    }
    
    HMapIntersection ret = INIT_INTERSECTION;
    ret.last_t = zTimesSc;
    ret.wasHit = (stepCount < steps);
    sc -= w;
    float tt = ds.z * sc;
    ret.uv = (1 - tt) * u + tt * u2;
    ret.t = tt;
    return ret;
}

// ray intersect depth map using relaxed cone stepping depth value stored in alpha channel (black is at object surface) and cone ratio stored in blue channel
void ray_intersect_relaxedcone(sampler2D relaxedcone_relief_map, inout float3 position_ts, inout float3 view_direction_ts) {
   const int cone_steps   = 32;
   const int binary_steps = 8;

   float3 p = position_ts;
   float3 v = view_direction_ts;

   float3 p0 = p;

   v /= v.z;

   float dist = length(v.xy);

   for (int i = 0; i < cone_steps; i++) {
      float4 tex = tex2D(relaxedcone_relief_map, p.xy);

      float height = saturate(tex.w - p.z);

      float cone_ratio = tex.z;

      p += v * (cone_ratio * height / (dist + cone_ratio));
   }

   v *= p.z * 0.5;
   p = p0 + v;

   for (int i = 0; i < binary_steps; i++) {
      float4 tex = tex2D(relaxedcone_relief_map, p.xy);
      v *= 0.5;
      if (p.z < tex.w) {
         p += v;
      }
      else {
         p -= v;
      }
   }
}

