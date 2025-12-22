#include <stdio.h>
#include <math.h>


void gaussian_kernel(float *w, int radius, float sigma) {
    float sum = 0.0f;

    for (int i = -radius; i <= radius; i++) {
        float v = expf(-(i*i) / (2.0f * sigma * sigma));
        w[i + radius] = v;
        sum += v;
    }

    for (int i = 0; i < 2*radius + 1; i++) {
        w[i] /= sum;
    }
}


void main() {
    float gk_arr[100];

    printf("// generated with mk_gaussian_kernels.c, assuming sigma = radius/3f\n");
    for (int radius = 1; radius <= 32; radius++) {
        float sigma = radius / 3.0f;
        gaussian_kernel(gk_arr, radius, sigma);


        printf("static float gaussian_kernel_%d[] = { ", radius);
        for (int i = 0; i < radius * 2 + 1; i++) {
            if (i > 0) printf(", ");
            printf("%f", gk_arr[i]);
        }
        printf("};\n");
    }
}
