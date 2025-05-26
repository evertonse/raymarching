/*
    "Hearts" by @XorDev
    
    Happy Valentine's Day
*/

void mainImage(out vec4 O,vec2 I)
{
    float i=0.0,t=iTime; // Original had 'i' not Initialized
    O *= i;
    for(vec2 a=iResolution.xy,p=(I+I-a)/a.y;i++<20.;
        O += (cos(sin(i*.2+t)*vec4(0,4,3,1))+2.)
        /(i/1e3+abs(length(a-.5*min(a+a.yx,.1))-.05)))
        a.x = abs(a=fract(.2*t+.3*p*i*mat2(cos(cos(.2*(t+i))+vec4(0,11,33,0))))-.5).x;
    O = tanh(O*O/2e5);
}
//Original [292]
/*
void mainImage( out vec4 O, in vec2 I )
{
    float i=0,t=iTime;
    O*=i;
    for(vec2 a=iResolution.xy,p=(I+I-a)/a.y;i++<20.;
        O+=(cos(sin(i*.2+t)*vec4(0,4,3,1))+2.)
        /(i/1e3+abs(length(a-.5*min(a+a.yx,.1))-.05)))
        
        a = fract(t*.2+p*.3*i*mat2(cos(cos(t*.2+i*.2)+vec4(0,11,33,0))))-.5,
        a = vec2(a.y, abs(a));
        
    O = tanh(O*O/2e5);
}
*/
