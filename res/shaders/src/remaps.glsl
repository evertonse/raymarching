
float remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd) {
   float result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
   return result;
}

float smooth_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   float t = clamp((value - in_min) / (in_max - in_min), 0.0, 1.0);
   t = t * t * (3.0 - 2.0 * t); // Smoothstep
   return lerp(out_min, out_max, t);
}

float pow_remap(float value, float in_min, float in_max, float out_min, float out_max, float gamma) {
   float t = clamp((value - in_min) / (in_max - in_min), 0.0, 1.0);
   t = pow(t, gamma); // gamma < 1: faster near, > 1: slower near
   return lerp(out_min, out_max, t);
}

float log_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   float z = clamp((value - in_min) / (in_max - in_min), 0.0001, 1.0);
   float log_z = log(z * 9.0 + 1.0) / log(10.0); // range still [0, 1]
   return lerp(out_min, out_max, log_z);
}


