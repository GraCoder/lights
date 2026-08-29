// Created by inigo quilez - iq/2013
// I share this piece (art and code) here in Shadertoy and through its Public API, only for educational purposes. 
// You cannot use, sell, share or host this piece or modifications of it as part of your own commercial or non-commercial product, website or project.
// You can share a link to it or an unmodified screenshot of it provided you attribute "by Inigo Quilez, @iquilezles and iquilezles.org". 
// If you are a teacher, lecturer, educator or similar and these conditions are too restrictive for your needs, please contact me and we'll work it out.

// See http://www.iquilezles.org/www/articles/warp/warp.htm for details


#if HW_PERFORMANCE==0
#define AA 1
#else
#define AA 2
#endif

// consider replacing this by a proper noise function
float noise( in vec2 x )
{
    vec2 p = floor(x);
    vec2 f = fract(x);
    f = f*f*(3.0-2.0*f);
    float a = textureLod(iChannel0,(p+vec2(0.5,0.5))/256.0,0.0).x;
	float b = textureLod(iChannel0,(p+vec2(1.5,0.5))/256.0,0.0).x;
	float c = textureLod(iChannel0,(p+vec2(0.5,1.5))/256.0,0.0).x;
	float d = textureLod(iChannel0,(p+vec2(1.5,1.5))/256.0,0.0).x;
    return mix(mix( a, b,f.x), mix( c, d,f.x),f.y);
}

const mat2 mtx = mat2( 0.80,  0.60, -0.60,  0.80 );

float fbm4( vec2 p )
{
    float f = 0.0;

    f += 0.5000*(-1.0+2.0*noise( p )); p = mtx*p*2.02;
    f += 0.2500*(-1.0+2.0*noise( p )); p = mtx*p*2.03;
    f += 0.1250*(-1.0+2.0*noise( p )); p = mtx*p*2.01;
    f += 0.0625*(-1.0+2.0*noise( p ));

    return f/0.9375;
}

float fbm6( vec2 p )
{
    float f = 0.0;

    f += 0.500000*noise( p ); p = mtx*p*2.02;
    f += 0.250000*noise( p ); p = mtx*p*2.03;
    f += 0.125000*noise( p ); p = mtx*p*2.01;
    f += 0.062500*noise( p ); p = mtx*p*2.04;
    f += 0.031250*noise( p ); p = mtx*p*2.01;
    f += 0.015625*noise( p );

    return f/0.96875;
}

vec2 fbm4_2( vec2 p )
{
    return vec2( fbm4(p+vec2(1.0)), fbm4(p+vec2(6.2)) );
}

vec2 fbm6_2( vec2 p )
{
    return vec2( fbm6(p+vec2(9.2)), fbm6(p+vec2(5.7)) );
}


float func( vec2 q, out vec2 o, out vec2 n )
{
    q += 0.05*sin(vec2(0.11,0.13)*iTime + length( q )*4.0);
    
    q *= 0.7 + 0.2*cos(0.05*iTime);

    o = 0.5 + 0.5*fbm4_2( q );
    
    o += 0.02*sin(vec2(0.11,0.13)*iTime*length( o ));

    n = fbm6_2( 4.0*o );

    vec2 p = q + 2.0*n + 1.0;

    float f = 0.5 + 0.5*fbm4( 2.0*p );

    f = mix( f, f*f*f*3.5, f*abs(n.x) );

    f *= 1.0-0.5*pow( 0.5+0.5*sin(8.0*p.x)*sin(8.0*p.y), 8.0 );

    return f;
}

float funcs( in vec2 q )
{
    vec2 t1, t2;
    return func(q,t1,t2);
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 tot = vec3(0.0);
#if AA>1
    for( int mi=0; mi<AA; mi++ )
    for( int ni=0; ni<AA; ni++ )
    {
        // pixel coordinates
        vec2 of = vec2(float(mi),float(ni)) / float(AA) - 0.5;
        vec2 q = (2.0*(fragCoord+of)-iResolution.xy)/iResolution.y;
#else    
        vec2 q = (2.0*fragCoord-iResolution.xy)/iResolution.y;
#endif

        vec2 o, n;
        float f = func(q, o, n);
        
        vec3 col = vec3(0.2,0.1,0.4);
        col = mix( col, vec3(0.3,0.05,0.05), f );
        col = mix( col, vec3(0.9,0.9,0.9), dot(n,n) );
        col = mix( col, vec3(0.5,0.2,0.2), 0.5*o.y*o.y );
        col = mix( col, vec3(0.0,0.2,0.4), 0.5*smoothstep(1.2,1.3,abs(n.y)+abs(n.x)) );
        col *= f*2.0;

        vec2 ex = vec2( 1.0 / iResolution.x, 0.0 );
        vec2 ey = vec2( 0.0, 1.0 / iResolution.y );
        #if AA>1
        ex /= float(AA);
        ey /= float(AA);
        #endif
        vec3 nor = normalize( vec3( funcs(q+ex) - f, ex.x, funcs(q+ey) - f ) );
        
        vec3 lig = normalize( vec3( 0.9, -0.2, -0.4 ) );
        float dif = clamp( 0.3+0.7*dot( nor, lig ), 0.0, 1.0 );

        vec3 bdrf;
        bdrf  = vec3(0.85,0.90,0.95)*(nor.y*0.5+0.5);
        bdrf += vec3(0.15,0.10,0.05)*dif;
        bdrf  = vec3(0.85,0.90,0.95)*(nor.y*0.5+0.5);
        bdrf += vec3(0.15,0.10,0.05)*dif;

        col *= bdrf;
        col = vec3(1.0)-col;
        col = col*col;
        col *= vec3(1.2,1.25,1.2);
        
        tot += col;
#if AA>1
    }
    tot /= float(AA*AA);
#endif

    
	vec2 p = fragCoord / iResolution.xy;
	tot *= 0.5 + 0.5 * sqrt(16.0*p.x*p.y*(1.0-p.x)*(1.0-p.y));
	
	fragColor = vec4( tot, 1.0 );
}





// Created by inigo quilez - iq/2013
// I share this piece (art and code) here in Shadertoy and through its Public API, only for educational purposes. 
// You cannot use, sell, share or host this piece or modifications of it as part of your own commercial or non-commercial product, website or project.
// You can share a link to it or an unmodified screenshot of it provided you attribute "by Inigo Quilez, @iquilezles and iquilezles.org". 
// If you are a techer, lecturer, educator or similar and these conditions are too restrictive for your needs, please contact me and we'll work it out.

// See here for a tutorial on how to make this:
//
// http://www.iquilezles.org/www/articles/warp/warp.htm

//====================================================================

const mat2 m = mat2( 0.80,  0.60, -0.60,  0.80 );

float noise( in vec2 p )
{
	return sin(p.x)*sin(p.y);
}

float fbm4( vec2 p )
{
    float f = 0.0;
    f += 0.5000*noise( p ); p = m*p*2.02;
    f += 0.2500*noise( p ); p = m*p*2.03;
    f += 0.1250*noise( p ); p = m*p*2.01;
    f += 0.0625*noise( p );
    return f/0.9375;
}

float fbm6( vec2 p )
{
    float f = 0.0;
    f += 0.500000*(0.5+0.5*noise( p )); p = m*p*2.02;
    f += 0.250000*(0.5+0.5*noise( p )); p = m*p*2.03;
    f += 0.125000*(0.5+0.5*noise( p )); p = m*p*2.01;
    f += 0.062500*(0.5+0.5*noise( p )); p = m*p*2.04;
    f += 0.031250*(0.5+0.5*noise( p )); p = m*p*2.01;
    f += 0.015625*(0.5+0.5*noise( p ));
    return f/0.96875;
}

vec2 fbm4_2( vec2 p )
{
    return vec2(fbm4(p), fbm4(p+vec2(7.8)));
}

vec2 fbm6_2( vec2 p )
{
    return vec2(fbm6(p+vec2(16.8)), fbm6(p+vec2(11.5)));
}

//====================================================================

float func( vec2 q, out vec4 ron )
{
    q += 0.03*sin( vec2(0.27,0.23)*iTime + length(q)*vec2(4.1,4.3));

	vec2 o = fbm4_2( 0.9*q );

    o += 0.04*sin( vec2(0.12,0.14)*iTime + length(o));

    vec2 n = fbm6_2( 3.0*o );

	ron = vec4( o, n );

    float f = 0.5 + 0.5*fbm4( 1.8*q + 6.0*n );

    return mix( f, f*f*f*3.5, f*abs(n.x) );
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 p = (2.0*fragCoord-iResolution.xy)/iResolution.y;
    float e = 2.0/iResolution.y;

    vec4 on = vec4(0.0);
    float f = func(p, on);

	vec3 col = vec3(0.0);
    col = mix( vec3(0.2,0.1,0.4), vec3(0.3,0.05,0.05), f );
    col = mix( col, vec3(0.9,0.9,0.9), dot(on.zw,on.zw) );
    col = mix( col, vec3(0.4,0.3,0.3), 0.2 + 0.5*on.y*on.y );
    col = mix( col, vec3(0.0,0.2,0.4), 0.5*smoothstep(1.2,1.3,abs(on.z)+abs(on.w)) );
    col = clamp( col*f*2.0, 0.0, 1.0 );
    
#if 0
    // gpu derivatives - bad quality, but fast
	vec3 nor = normalize( vec3( dFdx(f)*iResolution.x, 6.0, dFdy(f)*iResolution.y ) );
#else    
    // manual derivatives - better quality, but slower
    vec4 kk;
 	vec3 nor = normalize( vec3( func(p+vec2(e,0.0),kk)-f, 
                                2.0*e,
                                func(p+vec2(0.0,e),kk)-f ) );
#endif    

    vec3 lig = normalize( vec3( 0.9, 0.2, -0.4 ) );
    float dif = clamp( 0.3+0.7*dot( nor, lig ), 0.0, 1.0 );
    vec3 lin = vec3(0.70,0.90,0.95)*(nor.y*0.5+0.5) + vec3(0.15,0.10,0.05)*dif;
    col *= 1.2*lin;
	col = 1.0 - col;
	col = 1.1*col*col;
    
    fragColor = vec4( col, 1.0 );
}





////////////////////////////////////////////////////////////////////////////////////////////////
//fire//////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

float snoise(vec3 uv, float res)
{
    const vec3 s = vec3(1e0, 1e2, 1e3);

    uv *= res;

    vec3 uv0 = floor(mod(uv, res)) * s;
    vec3 uv1 = floor(mod(uv + vec3(1.), res)) * s;

    vec3 f = fract(uv); f = f * f * (3.0 - 2.0 * f);

    vec4 v = vec4(uv0.x + uv0.y + uv0.z, uv1.x + uv0.y + uv0.z,
        uv0.x + uv1.y + uv0.z, uv1.x + uv1.y + uv0.z);

    vec4 r = fract(sin(v * 1e-1) * 1e3);
    float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);

    r = fract(sin((v + uv1.z - uv0.z) * 1e-1) * 1e3);
    float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);

    return mix(r0, r1, f.z) * 2. - 1.;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 p = -.5 + fragCoord.xy / iResolution.xy;
    p.x *= iResolution.x / iResolution.y;

    float color = 3.0 - (3. * length(2. * p));

    vec3 coord = vec3(atan(p.x, p.y) / 6.2832 + .5, length(p) * .4, .5);

    for (int i = 1; i <= 7; i++)
    {
        float power = pow(2.0, float(i));
        color += (1.5 / power) * snoise(coord + vec3(0., -iTime * .05, iTime * .01), power * 16.);
    }
    fragColor = vec4(color, pow(max(color, 0.), 2.) * 0.4, pow(max(color, 0.), 3.) * 0.15, 1.0);
}

////////////////////////////////////////////////////////////////////////////////////////////////
//fire//////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
