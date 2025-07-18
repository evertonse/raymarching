/*
    "Hearts" by @XorDev modified
    
    Happy Valentine's Day
*/

void mainImage(out vec4 O, in vec2 I) {
   float i = 0, t = iTime;
   O *= i;
   for (
       vec2 a = iResolution.xy, p = (I + I - a) / a.y; 
       i++ < 20.;
       O +=
        (cos(sin(i * .2 + t) * vec4(0, 4, 3, 1)) + 2.)
       / (i / 1e3 + abs(length(a - .5 * min(a + a.yx, .1)) - .05))
   )
      a = fract(t * .2 + p * .3 * i * mat2(cos(cos(t * .2 + i * .2) + vec4(0, 11, 33, 0)))) - .5, a = vec2(a.y, abs(a));

   // O = tanh(O * O / 2e5);
   O = tanh(O*O/2e5*((cos(iTime)/2 ) + sin(iTime)));
}
