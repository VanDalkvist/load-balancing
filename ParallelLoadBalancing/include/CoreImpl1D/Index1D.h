#ifndef _INDEX1D_H
#define _INDEX1D_H

#include <Core/IIndex.h>

class Index1D : public IIndex
{
public:
	Index1D(int i);
	
	int operator[](int dimension) const;
	int Dimensions() const;

private:
	int i;
};

#endif // _INDEX1D_H