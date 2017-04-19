#ifndef MATHFUNCTIONS_H
#define MATHFUNCTIONS_H


#include "MainHeader.h"
#include <stdlib.h>
#include <OpenThreads/Thread>


#define PI 3.14159265


template <class T>
int round(T x)
{
	if (x>=0.5)
	{
		return ceil(x);
	}
	else
	{
		return floor(x);
	}
} 

template <class T> 
float mean(T x[], int vecL);

template <class T>
float stdev(T x[], int vecL);

template <class T>
float correlation(T x[], T y[], int vecL); 

void rotateCoordinates (float x, float y, float alpha, float * xprime, float * yprime);

void CartToPolar (float x, float y, float * rho, float * theta);

void PolarToCart (float rho, float theta, float * x, float * y);

// Mean Function
template <class T> 
float mean(T x[], int vecL)
{
	float mX(0);
	for (int i=0; i< vecL; i++)
	{
		mX+=x[i];
	}
	mX = mX/vecL;
	return (mX);
}

// Standard Deviation Function
template <class T> 
float stdev(T x[], int vecL)
{
	float mX = mean(x, vecL);
	float sX(0);
	for (int i=0; i< vecL; i++)
	{
		sX+=x[i]-mX;;
	}
	sX = sX/vecL;
	return (sX);
}

// Zero-lag correlation
template <class T> 
float correlation(T x[], T y[], int vecL)
{
	float mX = mean(x, vecL);
	float mY = mean(y, vecL);
	float sX = stdev(x, vecL);
	float sY = stdev(y, vecL);
	float cXY(0);
	for (int i=0; i< vecL; i++)
	{
		cXY+=(x[i]-mX)*(y[i]-mY);
	}
	cXY = cXY/(vecL*sX*sY);
	return (cXY);
}



#endif // MATHFUNCTIONS_H