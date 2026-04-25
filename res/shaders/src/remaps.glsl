float remap(float value, float in_min, float in_max, float out_min, float out_max) {
   const float v = value;
   return (v - in_min) / (in_max - in_min) * (out_max - out_min) + out_min;
}

float smooth_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   const float v = value;
   float t = clamp((v - in_min) / (in_max - in_min), 0.0, 1.0);
   t = t * t * (3.0 - 2.0 * t);
   return mix(out_min, out_max, t);
}

float pow_remap(float value, float in_min, float in_max, float out_min, float out_max, float gamma) {
   const float v = value;
   float t = clamp((v - in_min) / (in_max - in_min), 0.0, 1.0);
   t = pow(t, gamma);
   return mix(out_min, out_max, t);
}

float log_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   const float v = value;
   float t = clamp((v - in_min) / (in_max - in_min), 0.0001, 1.0);
   t = log(t * 9.0 + 1.0) / log(10.0);
   return mix(out_min, out_max, t);
}
