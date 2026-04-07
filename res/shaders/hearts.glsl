#version 460 core

layout(rgba32f, binding = 0) uniform writeonly image2D outputImage;
layout(local_size_x = 16, local_size_y = 16) in;
uniform float iTime;
uniform vec2 iResolution;

void main() {
  ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

  if (pixelCoord.x >= imageSize(outputImage).x ||
      pixelCoord.y >= imageSize(outputImage).y)
    return;

  ivec2 texSize = imageSize(outputImage);
  vec2 fTexSize = vec2(texSize);
  vec2 normalizedCoord = vec2(pixelCoord) / vec2(texSize);

  vec4 O = vec4(0, 0, 0, 1);
  vec2 I = vec2(pixelCoord);

  float i = 0.0, t = iTime;
  O *= i;
  for (vec2 a = fTexSize.xy, p = (I + I - a) / a.y; i++ < 20.;
       O += (cos(sin(i * .2 + t) * vec4(0, 4, 3, 1)) + 2.) /
            (i / 1e3 + abs(length(a - .5 * min(a + a.yx, .1)) - .05)))
    a.x = abs(a = (fract(.2 * t + .3 * p * i *
                                      mat2(cos(cos(.2 * t + .2 * i) +
                                               vec4(0, 11, 33, 0)))) -
                   .5))
              .x;

  O = tanh(O * O / 2e5);

  vec4 color = vec4(normalizedCoord, 0.0, 1.0);
  imageStore(outputImage, pixelCoord, O);
}
