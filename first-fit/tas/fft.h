#ifndef _FFT_H_
#define _FFT_H_

typedef struct {
	float real;
	float imag;
} complex_t;


int FFT2D_inplace(complex_t**,int,int,int);
int FFT2D(complex_t**,int,int,int, complex_t**);

#endif
