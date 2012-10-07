// ****************************************************************************
// LOAD BALANCING
// Copyright (C) 2012 Gerasimov, Smoryakova, Katerov, Afanasov, Kulakovich, Sobolev
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
// ****************************************************************************

#include "DomainModel.h"
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define SIZE_BLOCK 3

DomainModel::DomainModel(IInputFile& inputFile, IFunction& func, int steps)
	: inputFile(inputFile)
	, func(func)
	, steps(steps)
	, step(0)
{
}

void DomainModel::LoadProblem(IMPICommunicator& comm, IProblemBuilder& builder)
{
	int row_global;
	int col_global;
	int num_processor_row;
	int num_processor_col;
	
	int mpi_rank;
	comm.Rank(&mpi_rank);

	//��������� ���� ����� �������
	inputFile.Read(&row_global, sizeof(int), 1);
	inputFile.Read(&col_global, sizeof(int), 1);
	inputFile.Read(&num_processor_row, sizeof(int), 1);
	inputFile.Read(&num_processor_col, sizeof(int), 1);
		
	builder.SetBreakPointCount(num_processor_row - 1, num_processor_col - 1);

	int* solutionI = builder.CreateSolutionI();
	int* solutionJ = builder.CreateSolutionJ();

	inputFile.Read(solutionI, sizeof(int), num_processor_row + 1);
	inputFile.Read(solutionJ, sizeof(int), num_processor_col + 1);

	solutionI[0] = -1;
	solutionJ[0] = -1;

	// ����������� ���������� ����� ����������
	int ind_row = mpi_rank / num_processor_col;
	int ind_col = mpi_rank % num_processor_col;

	// ������ ������� ���� �����  ������ ����� � �������� (��������� � ��������)
	// ��� �������� ���������� (���� ��� "�����" �������� �������)
	int start_row;
	int finish_row;
	int start_col;
	int finish_col;
		
	// ��������� �� ind_row ���������� int
	//inputFile.Seek(ind_row * sizeof(int), SEEK_CUR);
	//inputFile.Read(&start_row, sizeof(int), 1);
	//if (ind_row != 0) 
	//	start_row++;				
	//inputFile.Read(&finish_row, sizeof(int), 1);
	//	
	//// ������ ��������� ������ ��������, ������� ������ �������
	//inputFile.Seek((num_processor_row + 1 + 4 + ind_col) * sizeof(int), SEEK_SET);
	//inputFile.Read(&start_col, sizeof(int), 1);		
	//if (ind_col != 0) 
	//	start_col++;
	//inputFile.Read(&finish_col, sizeof(int), 1);
	start_row = solutionI[ind_row];
	finish_row = solutionI[ind_row + 1];
	start_col = solutionJ[ind_col];
	finish_col = solutionJ[ind_col + 1];


	// ������ �������� ���� �������
	int row = finish_row - start_row;
	int col = finish_col - start_col;

	double* matrix = builder.CreateLocalMatrix();
		
	
	int tmp_row = start_row + 1;			// ����� ������� ������
	int count_variable_int = num_processor_row + 4 + num_processor_col + 2;
	for (int i = 0; i < row; i++)
	{
		// ���������� �� ������ �������
		inputFile.Seek((count_variable_int) * sizeof(int) + (tmp_row * col_global + start_col + 1) * sizeof(double), SEEK_SET);
		inputFile.Read(matrix + i * col, sizeof(double), col);
		tmp_row++;
	}
}

bool DomainModel::Run(
	IMPICommunicator& comm,
	int time_matrix[],
	const double matrix[],
	double new_matrix[],
	const int solutionI[], // bpNumberI + 2, solutionI[0] = -1, solutionI[bpNumberI + 1] = m - 1
	const int solutionJ[], // bpNumberJ + 2, solutionJ[0] = -1, solutionJ[bpNumberJ + 1] = n - 1
	int bpNumberI,
	int bpNumberJ)
{
	
	int mpi_rank;
	comm.Rank(&mpi_rank);

	int num_processor_row = bpNumberI + 1;
	int num_processor_col = bpNumberJ + 1;

	int procI = mpi_rank / num_processor_col;
	int procJ = mpi_rank % num_processor_col;

	
	int global_index_i = solutionI[procI] + 1;
	int global_index_j = solutionJ[procJ] + 1;

	int col = solutionJ[procJ + 1] - solutionJ[procJ];
	int row = solutionI[procI + 1] - solutionI[procI];

	

	clock_t start_time;
	clock_t finish_time; 

	Values values(SIZE_BLOCK);
	// ���� � ���� ��������� � ����� � ���� "��������"
	double* top_row = new double[col];		// ������� ������
	double* bottom_row = new double[col];	// ������� ������
	double* left_column = new double[row];	// ����� �������
	double* right_column = new double[row];	// ������ �������
	double top_left_corner = 0;				// ������� ����� ����
	double top_right_corner = 0;			// ������� ������ ����
	double bottom_left_corner = 0;			// ������ ����� ����
	double bottom_right_corner = 0;			// ������ ������ ����

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	Global_Sending(comm, col, row, num_processor_col, matrix, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ����������� "������������"
	for (int i = 1; i < row - 1; i++)
	{
		for (int j = 1; j < col - 1; j++)
		{	
			start_time = clock();
			// ��������� ������ ��� ������
			for (int k = 0; k < SIZE_BLOCK; k++)
			{
				for (int l = 0; l < SIZE_BLOCK; l++)
				{
					values.SetValue(k, l, matrix[(i - 1 + k) * col + j - 1 + l]);
				}
			}
			new_matrix[i * col + j] = func(values, global_index_i + i, global_index_j + j);
			finish_time = clock();
			time_matrix[i * col + j] = (int)(finish_time - start_time);
		}
	}

	// ����������� �������
	// ������ ������� ������
	int i = 0;
	for (int j = 0; j < col; j++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, &values, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(values, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}

	// ������ ��������� ������
	i = row - 1;
	for (int j = 0; j < col; j++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, &values, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(values, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}

	// ������ ������� �������
	int j = 0;
	for (int i = 1; i < row - 1; i++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, &values, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(values, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}

	// ������ ��������� �������
	j = col - 1;
	for (int i = 1; i < row - 1; i++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, &values, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(values, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// ������������ ������
	delete [] top_row;							// ������� ������
	delete [] bottom_row;						// ������� ������
	delete [] left_column;						// ����� �������
	delete [] right_column;						// ������ �������

	return ++step < steps;
}

void DomainModel::CreateArrSide(
	IMPICommunicator& comm,
	int col,
	int row,
	int num_processor_col,
	int num_processor_row,
	const double matrix[],
	Values* values,
	int i, int j, 
	double* top_row,						
	double* bottom_row,						
	double* left_column,					
	double* right_column,					
	double top_left_corner,					
	double top_right_corner,				
	double bottom_left_corner,				
	double bottom_right_corner
	)
{
	int mpi_rank;
	comm.Rank(&mpi_rank);

	int mpi_size;
	comm.Size(&mpi_size);

	// ������������� ����� ��� ��� �����, ������� ��� � �������� ������� ��� ������� ������
	bool is_top = (i == 0) && (mpi_rank < num_processor_col);
	bool is_left = (j == 0) && (mpi_rank % num_processor_col == 0);

	bool is_right = (j == col - 1) && ((mpi_rank + 1) % num_processor_col == 0);
	bool is_bottom = (i == row - 1) && (mpi_rank >= mpi_size - num_processor_col);

	///////////////////////////////////////////////////////////////////////////////////////
	values->SetValue(1, 1, matrix[i * col + j]);
	///////////////////////////////////////////////////////////////////////////////////////

	if (!(is_top || is_left)) 
	{
		if ((i == 0) && (j == 0))
		{
			values->SetValue(0, 0, top_left_corner);
		}
		else
		{
			if ((i == 0) && (j < col))
			{
				values->SetValue(0, 0, top_row[j - 1]);
			}
			else
				if ((j == 0) && (i < row))
				{
					values->SetValue(0, 0, left_column[i - 1]);
				}
				else
				{
					values->SetValue(0, 0, matrix[(i - 1) * col + j- 1]);
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!(is_top || is_right)) 
	{

		if ((i == 0) && (j == col - 1))
		{
			values->SetValue(0, 2, top_right_corner);
		}
		else
		{
			if ((i == 0) && (j < col))
			{
				values->SetValue(0, 2, top_row[j + 1]);
			}
			else
				if ((j == col - 1) && (i > 0))
				{
					values->SetValue(0, 2, right_column[i - 1]);
				}
				else
				{
					values->SetValue(0, 2, matrix[(i - 1) * col + j + 1]);
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!(is_bottom || is_left)) 
	{
		if ((i == row - 1) && (j == 0))
		{
			values->SetValue(2, 0, bottom_left_corner);
		}
		else
		{
			if ((i == row - 1) && (j > 0))
			{
				values->SetValue(2, 0, bottom_row[j - 1]);
			}
			else
				if ((j == 0) && (i < row))
				{
					values->SetValue(2, 0, left_column[i + 1]);
				}
				else
				{
					values->SetValue(2, 0, matrix[(i + 1) * col + j - 1]);
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!(is_bottom || is_right)) 
	{
		if ((i == row - 1) && (j == col - 1))
		{
			values->SetValue(2, 2, bottom_right_corner);
		}
		else
		{
			if ((i == row - 1) && (j < col))
			{
				values->SetValue(2, 2, bottom_row[j + 1]);
			}
			else
				if ((j == col - 1) && (i < row))
				{
					values->SetValue(2, 2, right_column[i + 1]);
				}
				else
				{
					values->SetValue(2, 2, matrix[(i + 1) * col + j + 1]);
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!is_left)
	{
		if (j == 0)
		{
			values->SetValue(1, 0, left_column[i]);
		}
		else
		{
			values->SetValue(1, 0, matrix[i * col + j - 1]);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!is_top)
	{
		if (i == 0)
		{
			values->SetValue(0, 1, top_row[j]);
		}
		else
		{
			values->SetValue(0, 1, matrix[(i - 1) * col + j]);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!is_bottom) 
	{
		if (i == row - 1)
		{
			values->SetValue(2, 1, bottom_row[j]);
		}
		else
		{
			values->SetValue(2, 1, matrix[(i + 1) * col + j]);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (!is_right) 
	{
		if (j == col - 1)
		{
			values->SetValue(1, 2, right_column[i]);
		}
		else
		{
			values->SetValue(1, 2, matrix[i * col + j + 1]);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	
	if(is_left)
		values->SetOffsetJ(SIZE_BLOCK / 2);
	else if(is_right)
		values->SetOffsetJ(-SIZE_BLOCK / 2);
	else
		values->SetOffsetJ(0);
	
	if(is_top)
		values->SetOffsetI(SIZE_BLOCK / 2);
	else if(is_bottom)
		values->SetOffsetI(-SIZE_BLOCK / 2);
	else
		values->SetOffsetI(0);
}

void DomainModel::Global_Sending(	
	IMPICommunicator& comm,
	int col,
	int row,
	int num_processor_col,
	const double matrix[],
	double* top_row,						// ������� ������
	double* bottom_row,						// ������� ������
	double* left_column,					// ����� �������
	double* right_column,					// ������ �������
	double &top_left_corner,				// ������� ����� ����
	double &top_right_corner,				// ������� ������ ����
	double &bottom_left_corner,				// ������ ����� ����
	double &bottom_right_corner				// ������ ������ ����
	)
{
	int mpi_rank;
	comm.Rank(&mpi_rank);
	int mpi_size;
	comm.Size(&mpi_size);
	MPI_Status status;

	// ��� � ���� ��������
	const double* top_row_send;					// ������� ������
	const double* bottom_row_send ;				// ������� ������
	double* left_column_send = new double[row];	// ����� �������
	double* right_column_send = new double[row];// ������ �������
	double top_left_corner_send = 0;			// ������� ����� ����
	double top_right_corner_send = 0;			// ������� ������ ����
	double bottom_left_corner_send = 0;			// ������ ����� ����
	double bottom_right_corner_send = 0;		// ������ ������ ����

	// ��������� ��, ��� ������ �������, �������� � ���������
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	top_row_send = matrix; //������� �������

	if (mpi_rank < num_processor_col) 
		// ����������� ������ ����-������ ����� ������ �������� ������ �������
	{
		comm.Recv((void*) bottom_row, col, MPI_DOUBLE, mpi_rank + num_processor_col, 0, &status);
	}
	else 
	{
		if(mpi_rank >= mpi_size - num_processor_col) 
			// ����������� ��������� ����-������ �� ����� ������ �������, 
			// ������ �������� ���� ������� �������
		{
			comm.Send((void*) top_row_send, col, MPI_DOUBLE, mpi_rank - num_processor_col, 0);
		}
		else
		{
			comm.Recv((void*) bottom_row, col, MPI_DOUBLE, mpi_rank + num_processor_col, 0, &status);
			comm.Send((void*) top_row_send, col, MPI_DOUBLE, mpi_rank - num_processor_col, 0);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bottom_row_send = matrix + (row - 1) * col; // ������ �������

	if (mpi_rank >= mpi_size - num_processor_col) 
		// ����������� ��������� ����-������ ����� ������ �������� ������� �������
	{
		comm.Recv((void*) top_row, col, MPI_DOUBLE, mpi_rank - num_processor_col, 0, &status);
	}
	else 
	{
		if(mpi_rank < num_processor_col) 
			// ����������� ������ ����-������ �� ����� ������� �������, 
			// ������ �������� ���� ������ �������
		{
			comm.Send((void*) bottom_row_send, col, MPI_DOUBLE, mpi_rank + num_processor_col, 0);
		}
		else
		{
			comm.Recv((void*) top_row, col, MPI_DOUBLE, mpi_rank - num_processor_col, 0, &status);
			comm.Send((void*) bottom_row_send, col, MPI_DOUBLE, mpi_rank + num_processor_col, 0);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	if((mpi_rank + 1) % num_processor_col != 0)
	{
		for (int i = 0; i < row; i++)
		{
			right_column_send[i] = matrix[i * col + col - 1];
		}
	}

	
	if ((mpi_rank + 1) % num_processor_col == 0) // ������ �������
	{
		comm.Recv((void*) left_column, row, MPI_DOUBLE, mpi_rank - 1, 0, &status);
	}
	else 
	{
		if(mpi_rank % num_processor_col == 0) // ����� �������
		{
			comm.Send((void*) right_column_send, row, MPI_DOUBLE, mpi_rank + 1, 0);
		}
		else
		{
			comm.Recv((void*) left_column, row, MPI_DOUBLE, mpi_rank - 1, 0, &status);
			comm.Send((void*) right_column_send, row, MPI_DOUBLE, mpi_rank + 1, 0);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	if(mpi_rank % num_processor_col != 0)
	{
		for (int i = 0; i < row; i++)
		{
			left_column_send[i] = matrix[i * col];
		}
	}

	if (mpi_rank % num_processor_col == 0) // ����� �������
	{
		comm.Recv((void*) right_column, row, MPI_DOUBLE, mpi_rank + 1, 0, &status);
	}
	else 
	{
		if((mpi_rank + 1) % num_processor_col == 0) // ������ �������
		{
			comm.Send((void*) left_column_send, row, MPI_DOUBLE, mpi_rank - 1, 0);
		}
		else
		{
			comm.Recv((void*) right_column, row, MPI_DOUBLE, mpi_rank + 1, 0, &status);
			comm.Send((void*) left_column_send, row, MPI_DOUBLE, mpi_rank - 1, 0);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	top_left_corner_send = matrix[0];

	if ((mpi_rank % num_processor_col == 0)  ||	(mpi_rank < num_processor_col )) // ����� ������� ��� ������� �������
	{
		if ((mpi_rank != mpi_size - num_processor_col) && (mpi_rank != num_processor_col - 1))
		{
			comm.Recv((void*) &bottom_right_corner, 1, MPI_DOUBLE, mpi_rank + num_processor_col + 1, 0, &status);
		}
		
	}
	else 
	{
		if(((mpi_rank + 1) % num_processor_col == 0) || (mpi_rank >= mpi_size - num_processor_col)) // ������ ������� + ������ �������
		{
			comm.Send((void*) &top_left_corner_send, 1, MPI_DOUBLE, mpi_rank - num_processor_col - 1, 0);
		}
		else
		{
			comm.Recv((void*) &bottom_right_corner, 1, MPI_DOUBLE, mpi_rank + num_processor_col + 1, 0, &status);
			comm.Send((void*) &top_left_corner_send, 1, MPI_DOUBLE, mpi_rank - num_processor_col - 1, 0);
		}
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	top_right_corner_send = matrix[col - 1];

	if (((mpi_rank + 1) % num_processor_col == 0)  || (mpi_rank < num_processor_col )) // ������ ������� ��� ������� �������
	{
		if ((mpi_rank != 0) && (mpi_rank != mpi_size - 1))
		{
			comm.Recv((void*) &bottom_left_corner, 1, MPI_DOUBLE, mpi_rank + num_processor_col - 1, 0, &status);
		}
		
	}
	else 
	{
		if((mpi_rank % num_processor_col == 0) || (mpi_rank >= mpi_size - num_processor_col)) // ������ ������� + ������ �������
		{
			comm.Send((void*) &top_right_corner_send, 1, MPI_DOUBLE, mpi_rank - num_processor_col + 1, 0);
		}
		else
		{
			comm.Recv((void*) &bottom_left_corner, 1, MPI_DOUBLE, mpi_rank + num_processor_col - 1, 0, &status);
			comm.Send((void*) &top_right_corner_send, 1, MPI_DOUBLE, mpi_rank - num_processor_col + 1, 0);
		}
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	bottom_left_corner_send = matrix[(row - 1) * col];
	if ((mpi_rank  % num_processor_col == 0)  || (mpi_rank >= mpi_size - num_processor_col)) // ����� ������� ��� ������ �������
	{
		if ((mpi_rank != 0) && (mpi_rank != mpi_size - 1))
		{
			comm.Recv((void*) &top_right_corner, 1, MPI_DOUBLE, mpi_rank - num_processor_col + 1, 0, &status);
		}
		
	}
	else 
	{
		if(((mpi_rank + 1) % num_processor_col == 0) || (mpi_rank < num_processor_col)) // ������ ������� + ������� �������
		{
			comm.Send((void*) &bottom_left_corner_send, 1, MPI_DOUBLE, mpi_rank + num_processor_col - 1, 0);
		}
		else
		{
			comm.Recv((void*) &top_right_corner, 1, MPI_DOUBLE, mpi_rank - num_processor_col + 1, 0, &status);
			comm.Send((void*) &bottom_left_corner_send, 1, MPI_DOUBLE, mpi_rank + num_processor_col - 1, 0);
		}
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	bottom_right_corner_send = matrix[(row - 1) * col + col - 1];
	if (((mpi_rank + 1)  % num_processor_col == 0)  || (mpi_rank >= mpi_size - num_processor_col)) // ������ ������� ��� ������ �������
	{
		if ((mpi_rank != num_processor_col - 1) && (mpi_rank != mpi_size - num_processor_col))
		{
			comm.Recv((void*) &top_left_corner, 1, MPI_DOUBLE, mpi_rank - num_processor_col - 1, 0, &status);
		}
		
	}
	else 
	{
		if((mpi_rank % num_processor_col == 0) || (mpi_rank < num_processor_col)) // ����� ������� + ������� �������
		{
			comm.Send((void*) &bottom_right_corner_send, 1, MPI_DOUBLE, mpi_rank + num_processor_col + 1, 0);
		}
		else
		{
			comm.Recv((void*) &top_left_corner, 1, MPI_DOUBLE, mpi_rank - num_processor_col - 1, 0, &status);
			comm.Send((void*) &bottom_right_corner_send, 1, MPI_DOUBLE, mpi_rank + num_processor_col + 1, 0);
		}
	}
	
	// ������������ ������
	top_row_send = NULL;						// ������� ������
	bottom_row_send = NULL;						// ������� ������
	delete [] left_column_send;					// ����� �������
	delete [] right_column_send;				// ������ �������
}
