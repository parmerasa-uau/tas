#include <stdlib.h>
#include <math.h>
#include "fft.h"
#include <stdio.h>

#define TWOPI	(2.0*M_PI)

/*
original comment:
 FFT/IFFT routine. (see pages 507-508 of Numerical Recipes in C)

 Inputs:
	data[] : array of complex* data points of size 2*NFFT+1.
		data[0] is unused,
		* the n'th complex number x(n), for 0 <= n <= length(x)-1, is stored as:
			data[2*n+1] = real(x(n))
			data[2*n+2] = imag(x(n))
		if length(Nx) < NFFT, the remainder of the array must be padded with zeros

	nn : FFT order NFFT. This MUST be a power of 2 and >= length(x).
	isign:  if set to 1, 
				computes the forward FFT
			if set to -1, 
				computes Inverse FFT - in this case the output values have
				to be manually normalized by multiplying with 1/NFFT.
 Outputs:
	data[] : The FFT or IFFT results are stored in data, overwriting the input.
*/
void four1(int isign, int nn, float *real, float *imag) {
	int n, mmax, m, j, istep, i;
	float wtemp, wr, wpr, wpi, wi, theta;
	float tempr, tempi;

	n = nn << 1;
	j = 1;
	for (i = 1; i < n; i += 2) {
		if (j > i) {
			int j2 = (j-1)/2;
			int i2 = (i-1)/2;
			//tempr = data[j];
			tempr = real[j2];
			//data[j] = data[i];
			real[j2] = real[i2];
			//data[i] = tempr;
			real[i2] = tempr;
			//tempr = data[j+1];
			tempr = imag[j2];
			//data[j+1] = data[i+1];
			imag[j2] = imag[i2];
			//data[i+1] = tempr;
			imag[i2] = tempr;
		}
		m = n >> 1;
		while (m >= 2 && j > m) {
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	mmax = 2;
	while (n > mmax) {
		istep = 2*mmax;
		theta = TWOPI/(isign*mmax);
		wtemp = sin(0.5*theta);
		wpr = -2.0*wtemp*wtemp;
		wpi = sin(theta);
		wr = 1.0;
		wi = 0.0;
		for (m = 1; m < mmax; m += 2) {
			for (i = m; i <= n; i += istep) {
				j =i + mmax;
				int j2 = (j-1)/2;
				int i2 = (i-1)/2;
				//tempr = wr*data[j]   - wi*data[j+1];
				tempr = wr*real[j2] - wi*imag[j2];
				//tempi = wr*data[j+1] + wi*data[j];
				tempi = wr*imag[j2] + wi*real[j2];
				//data[j]   = data[i]   - tempr;
				real[j2] = real[i2] - tempr;
				//data[j+1] = data[i+1] - tempi;
				imag[j2] = imag[i2] - tempi;
				//data[i] += tempr;
				real[i2] += tempr;
				//data[i+1] += tempi;
				imag[i2] += tempi;
			}
			wr = (wtemp = wr)*wpr - wi*wpi + wr;
			wi = wi*wpr + wtemp*wpi + wi;
		}
		mmax = istep;
	}
}


int isPowerOfTwo (unsigned int x) {
	unsigned int numberOfOneBits = 0;

	while(x && numberOfOneBits <=1) {
		if ((x & 1) == 1) { // Is the least significant bit a 1?
			numberOfOneBits++;
		}
		x >>= 1;//Shift number one bit to the right
	}

	return (numberOfOneBits == 1); // 'True' if only one 1 bit
}

/*-------------------------------------------------------------------------
   Perform a 2D FFT inplace given a complex 2D array
   The direction dir, 1 for forward, -1 for reverse
   The size of the array (nx,ny)
   Return 0 if there are memory problems or
      the dimensions are not powers of 2
*/
int FFT2D_inplace(complex_t** c,int nx,int ny,int dir) {
	int i,j;
	float real[nx], imag[nx];

	if(!isPowerOfTwo(nx) || !isPowerOfTwo(ny)) {
		return 0;
	}

	for (j=0;j<ny;j++) {
		for (i=0;i<nx;i++) {
			real[i] = c[i][j].real;
			imag[i] = c[i][j].imag;
		}
		four1(dir,nx,real,imag);
		for (i=0;i<nx;i++) {
			c[i][j].real = real[i];
			c[i][j].imag = imag[i];
		}
	}

	/* Transform the columns */
	if (real == NULL || imag == NULL) {
		return 0;
	}
	for (i=0;i<nx;i++) {
		for (j=0;j<ny;j++) {
			real[j] = c[i][j].real;
			imag[j] = c[i][j].imag;
		}
		four1(dir,ny,real,imag);
		for (j=0;j<ny;j++) {
			c[i][j].real = real[j];
			c[i][j].imag = imag[j];
		}
	}

	return 1;
}

int FFT2D(complex_t** c,int nx,int ny,int dir, complex_t** out) {
	int i,j;
	float real[nx],imag[nx];

	if(!isPowerOfTwo(nx) || !isPowerOfTwo(ny)) {
		return 0;
	}

	for (j=0;j<ny;j++) {
		for (i=0;i<nx;i++) {
			real[i] = c[i][j].real;
			imag[i] = c[i][j].imag;
		}
		four1(dir,nx,real,imag);
		for (i=0;i<nx;i++) {
			out[i][j].real = real[i];
			out[i][j].imag = imag[i];
		}
	}

	/* Transform the columns */
	if (real == NULL || imag == NULL) {
		return 0;
	}
	for (i=0;i<nx;i++) {
		for (j=0;j<ny;j++) {
			real[j] = out[i][j].real;
			imag[j] = out[i][j].imag;
		}
		four1(dir,ny,real,imag);
		for (j=0;j<ny;j++) {
			out[i][j].real = real[j];
			out[i][j].imag = imag[j];
		}
	}


	return 1;
}

