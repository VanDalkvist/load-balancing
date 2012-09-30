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

#include "TestingSystem.h"
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define SIZE_BLOCK 3
#define PI 3.141592653589793238462643
#define LIMIT(arr, limit) ((arr > 10.0) ? 10.0 : arr)

TestingSystem::TestingSystem(IInputFile& inputFile, int steps)
	: inputFile(inputFile)
	, steps(steps)
	, step(0)
{
}

void TestingSystem::LoadProblem(IMPICommunicator& comm, IProblemBuilder& builder)
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

bool TestingSystem::Run(
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

	double** arr = new double*[SIZE_BLOCK];
	bool** flag_arr = new bool*[SIZE_BLOCK];
	for (int i = 0; i < SIZE_BLOCK; i++)
	{
		arr[i] = new double[SIZE_BLOCK];
		flag_arr[i] = new bool[SIZE_BLOCK];
	}
	
	// �������, ����  ����� ������ - ���� ��� 8 �������� �����
	for (int i = 0; i < SIZE_BLOCK; i++)
		for (int j = 0; j < SIZE_BLOCK; j++)
		{
			flag_arr[i][j] = true;
		}
	
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
			CreateArr(matrix, col, arr, i, j);			
			new_matrix[i * col + j] = func(arr, flag_arr, global_index_i + i, global_index_j + j);
			finish_time = clock();
			time_matrix[i * col + j] = (int)(finish_time - start_time);
		}
	}

	// ����������� �������
	// ������ ������� ������
	int i = 0;
	for (int j = 0; j < col; j++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, arr, flag_arr, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(arr, flag_arr, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}

	// ������ ��������� ������
	i = row - 1;
	for (int j = 0; j < col; j++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, arr, flag_arr, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(arr, flag_arr, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}

	// ������ ������� �������
	int j = 0;
	for (int i = 1; i < row - 1; i++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, arr, flag_arr, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(arr, flag_arr, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}

	// ������ ��������� �������
	j = col - 1;
	for (int i = 1; i < row - 1; i++)
	{
		CreateArrSide(comm, col, row, num_processor_col, num_processor_row, matrix, arr, flag_arr, i, j, top_row,	bottom_row,	left_column, right_column, top_left_corner, top_right_corner, bottom_left_corner, bottom_right_corner);
		start_time = clock();
		new_matrix[i * col + j] = func(arr, flag_arr, global_index_i + i, global_index_j + j);
		finish_time = clock();
		time_matrix[i * col + j] = (int)(finish_time - start_time);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// ������������ ������
	delete [] top_row;							// ������� ������
	delete [] bottom_row;						// ������� ������
	delete [] left_column;						// ����� �������
	delete [] right_column;						// ������ �������
	for (int i = 0; i < SIZE_BLOCK; i++)
	{
		delete [] arr[i];
		delete [] flag_arr[i];
	}
	delete [] arr;
	delete [] flag_arr;

	return ++step < steps;
}

// ��������� �������� ������� � ����� (i, j)
double TestingSystem::func(double **arr, bool **flag_arr, int global_i, int global_j)
{
	// ���������
	double sum = 0.0;
	double coef[SIZE_BLOCK][SIZE_BLOCK];

	coef[0][0] = 1 / 4 * (global_i - 1) * (global_i - 2) * (global_j - 1) * (global_j - 2) * (global_j + 1);
	coef[0][1] = -1 / 4 * global_i * (global_i + 1) * (global_i - 2) * (global_j - 1) * (global_j - 2) * (global_j + 1);
	coef[0][2] = -1 / 4 * global_j * (global_i - 1) * (global_i - 2) * (global_i + 1) * (global_j + 1) * (global_j - 2);
	coef[1][0] = -1 / 12 * global_i * (global_i - 1) * (global_i - 2) * (global_j - 1) * (global_j - 2) * (global_j + 1);
	coef[1][1] = -1 / 12 * global_j * (global_i - 1) * (global_i - 2) * (global_i + 1) * (global_j - 1) * (global_j - 2);
	coef[1][2] = 1 / 36 * global_i * global_j * (global_i - 1) * (global_i - 2) * (global_j - 1) * (global_j + 2);
	coef[2][0] = -1 / 36 * global_i * global_j * (global_i - 1) * (global_i - 2) * (global_j - 1) * (global_j + 1);
	coef[2][1] = 1 / 12 * global_i * global_j * (global_i - 1) * (global_i - 2) * (global_j + 1) * (global_j - 2);
	coef[2][2] = -1 / 12 * global_i * global_j * (global_i + 1) * (global_i - 2) * (global_j - 1) * (global_j + 1);

	
	
	int count_elem = 0;
	for (int i = 0; i < SIZE_BLOCK; i++)
		for (int j = 0; j < SIZE_BLOCK; j++)
			if (flag_arr[i][j])
			{
				count_elem ++;						
			}

	int count = 1 + (LIMIT(arr[SIZE_BLOCK / 2][SIZE_BLOCK / 2] / 2.0, 1.0)) * 3;
	
	for (int k = 0; k < count; k++)
		for (int i = 0; i < SIZE_BLOCK; i++)
			for (int j = 0; j < SIZE_BLOCK; j++)
				if (flag_arr[i][j])
				{
					sum += abs(cos(coef[i][j] * tanh(sin(PI * LIMIT(arr[i][j], 10.0))) + sinh(LIMIT(arr[i][j], 10.0)) + cosh(LIMIT(arr[i][j], 10.0)) + tanh(LIMIT(LIMIT(PI * arr[i][j], 10.0), 10))));
				}

	sum /= count_elem;	
	return sum;
}

void TestingSystem::CreateArr(const double matrix[], int col, double **arr, int i, int j)
{
	
	for (int k = 0; k < SIZE_BLOCK; k++)
		for (int l = 0; l < SIZE_BLOCK; l++)
		{
			arr[k][l] = matrix[(i - 1 + k) * col + j - 1 + l];
		}	
}

void TestingSystem::CreateArrSide(
	IMPICommunicator& comm,
	int col,
	int row,
	int num_processor_col,
	int num_processor_row,
	const double matrix[],
	double** arr, 
	bool** flag_arr, 
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

	for (int i = 0; i < SIZE_BLOCK; i++)
	{
		for (int j = 0; j < SIZE_BLOCK; j++)
		{
			flag_arr[i][j] = true;
		}
	}

	// ������������� ����� ��� ��� �����, ������� ��� � �������� ������� ��� ������� ������
	if ((i == 0) && (mpi_rank < num_processor_col))
	{
		for (int k = 0; k < SIZE_BLOCK; k++)
			flag_arr[0][k] = false;
	}

	if ((j == 0) && (mpi_rank % num_processor_col == 0))
	{
		for (int k = 0; k < SIZE_BLOCK; k++)
			flag_arr[k][0] = false;
	}

	if ((j == col - 1) && ((mpi_rank + 1) % num_processor_col == 0))
	{
		for (int k = 0; k < SIZE_BLOCK; k++)
			flag_arr[k][SIZE_BLOCK - 1] = false;
	}

	if ((i == row - 1) && (mpi_rank >= mpi_size - num_processor_col))
	{
		for (int k = 0; k < SIZE_BLOCK; k++)
			flag_arr[SIZE_BLOCK - 1][k] = false;
	}
	///////////////////////////////////////////////////////////////////////////////////////
	arr[1][1] = matrix[i * col + j];
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[0][0]) 
	{
		if ((i == 0) && (j == 0))
		{
			arr[0][0] = top_left_corner;
		}
		else
		{
			if ((i == 0) && (j < col))
			{
				arr[0][0] = top_row[j - 1];
			}
			else
				if ((j == 0) && (i < row))
				{
					arr[0][0] = left_column[i - 1];
				}
				else
				{
					arr[0][0] = matrix[(i - 1) * col + j- 1];
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[0][2]) 
	{
		if ((i == 0) && (j == col - 1))
		{
			arr[0][2] = top_right_corner;
		}
		else
		{
			if ((i == 0) && (j < col))
			{
				arr[0][2] = top_row[j + 1];
			}
			else
				if ((j == col - 1) && (i > 0))
				{
					arr[0][2] = right_column[i - 1];
				}
				else
				{
					arr[0][2] = matrix[(i - 1) * col + j + 1];
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[2][0]) 
	{
		if ((i == row - 1) && (j == 0))
		{
			arr[2][0] = bottom_left_corner;
		}
		else
		{
			if ((i == row - 1) && (j > 0))
			{
				arr[2][0] = bottom_row[j - 1];
			}
			else
				if ((j == 0) && (i < row))
				{
					arr[2][0] = left_column[i + 1];
				}
				else
				{
					arr[2][0] = matrix[(i + 1) * col + j - 1];
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[2][2]) 
	{
		if ((i == row - 1) && (j == col - 1))
		{
			arr[2][2] = bottom_right_corner;
		}
		else
		{
			if ((i == row - 1) && (j < col))
			{
				arr[2][2] = bottom_row[j + 1];
			}
			else
				if ((j == col - 1) && (i < row))
				{
					arr[2][2] = right_column[i + 1];
				}
				else
				{
					arr[2][2] = matrix[(i + 1) * col + j + 1];
				}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[1][0]) 
	{
		if (j == 0)
		{
			arr[1][0] = left_column[i];
		}
		else
		{
			arr[1][0] = matrix[i * col + j - 1];
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[0][1]) 
	{
		if (i == 0)
		{
			arr[0][1] = top_row[j];
		}
		else
		{
			arr[0][1] = matrix[(i - 1) * col + j];
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[2][1]) 
	{
		if (i == row - 1)
		{
			arr[2][1] = bottom_row[j];
		}
		else
		{
			arr[2][1] = matrix[(i + 1) * col + j];
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////
	if (flag_arr[1][2]) 
	{
		if (j == col - 1)
		{
			arr[1][2] = right_column[i];
		}
		else
		{
			arr[1][2] = matrix[i * col + j + 1];
		}
	}		
}

void TestingSystem::Global_Sending(	
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
