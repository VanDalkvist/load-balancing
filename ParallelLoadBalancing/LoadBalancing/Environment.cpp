#include "Environment.h"

#include "ProblemBuilder.h"

#include <stdlib.h>
#include <memory.h>

void Environment::Run(IMPICommunicator& comm, ITestingSystem& ts, ILoadBalancingAlgorithm& lb, IRebalancer& rb)
{
	int mpi_size;
	int mpi_rank;

	comm.Size(&mpi_size);
	comm.Rank(&mpi_rank);
	
	int currentSolution = 0;
	int currentMatrix = 0;

	int* solutionI[2];
	int* solutionJ[2];
	double* matrix[2];
	int bpNumberI;
	int bpNumberJ;

	ProblemBuilder pb(comm, &solutionI[currentSolution], &solutionJ[currentSolution], &matrix[currentMatrix], &bpNumberI, &bpNumberJ, malloc);
	ts.LoadProblem(comm, pb);
	
	int procI = mpi_rank / (bpNumberJ + 1);
	int procJ = mpi_rank % (bpNumberJ + 1);

	int matrixHeight = solutionI[currentSolution][procI + 1] - solutionI[currentSolution][procI];
	int matrixWidth = solutionJ[currentSolution][procJ + 1] - solutionJ[currentSolution][procJ];
		
	solutionI[(currentSolution + 1) % 2] = (int*)malloc(sizeof(int) * (bpNumberI + 2));
	solutionJ[(currentSolution + 1) % 2] = (int*)malloc(sizeof(int) * (bpNumberJ + 2));
	
	matrix[(currentMatrix + 1) % 2] = (double*)malloc(sizeof(double) * matrixHeight * matrixWidth);
	int *time_matrix = (int*)malloc(sizeof(int) * matrixHeight * matrixWidth);

	while(ts.Run(comm, time_matrix, matrix[currentMatrix], matrix[(currentMatrix + 1) % 2], solutionI[currentSolution], solutionJ[currentSolution], bpNumberI, bpNumberJ))
	{
		currentMatrix = (currentMatrix + 1) % 2;

		if(printResults)
		{
			int _;

			if(procJ == 0)
			{
				if(procI > 0)
				{
					comm.Recv(&_, 1, MPI_INTEGER, mpi_rank - 1, 0, NULL);
				}
				else
				{
					printf("before\n");
				}
			}

			for(int i = 0; i < matrixHeight; i++)
			{
				if(procJ > 0)
				{
					comm.Recv(&_, 1, MPI_INTEGER, mpi_rank - 1, 0, NULL);
				}
				else if(i > 0)
				{
					comm.Recv(&_, 1, MPI_INTEGER, mpi_rank + bpNumberJ, 0, NULL);
				}

				for(int j = 0; j < matrixWidth; j++)
				{
					printf("%-8lg ", matrix[currentMatrix][i * matrixWidth + j]);
				}
	
				if(procJ < bpNumberJ)
				{
					comm.Send(&_, 1, MPI_INTEGER, mpi_rank + 1, 0);
				}
				else
				{
					printf("\n");
				
					if(i < matrixHeight - 1)
					{
				
						comm.Send(&_, 1, MPI_INTEGER, mpi_rank - bpNumberJ, 0);
					}
				}
			}
	
			if(procI < bpNumberI && procJ == bpNumberJ)
			{
				comm.Send(&_, 1, MPI_INTEGER, mpi_rank + 1, 0);
			}
		}

		if(needLoadBalancing)
		{
			int newSolution = (currentSolution + 1) % 2;
			memcpy(solutionI[newSolution], solutionI[currentSolution], (bpNumberI + 2) * sizeof(int));
			memcpy(solutionJ[newSolution], solutionJ[currentSolution], (bpNumberJ + 2) * sizeof(int));

			lb.Run(comm,
				time_matrix,
				solutionI[currentSolution], solutionJ[currentSolution],
				bpNumberI, bpNumberJ,
				solutionI[newSolution], solutionJ[newSolution]
				);
			
			printf("%d %d\n", solutionI[currentSolution][1], solutionJ[currentSolution][1]);
			printf("%d %d\n", solutionI[newSolution][1],     solutionJ[newSolution][1]);

			int newMatrix = (currentMatrix + 1) % 2;
			int newMatrixHeight = solutionI[newSolution][procI + 1] - solutionI[newSolution][procI];
			int newMatrixWidth = solutionJ[newSolution][procJ + 1] - solutionJ[newSolution][procJ];

			if(newMatrixHeight * newMatrixWidth != matrixHeight * matrixWidth)
			{
				matrix[newMatrix]     = (double*)realloc(matrix[newMatrix], sizeof(double) * newMatrixHeight * newMatrixWidth);
			}

			rb.Rebalance(comm, solutionI[currentSolution], solutionJ[currentSolution], matrix[currentMatrix], solutionI[newSolution], solutionJ[newSolution], matrix[newMatrix], bpNumberI, bpNumberJ);
			
			currentSolution = newSolution;

			if(newMatrixHeight * newMatrixWidth != matrixHeight * matrixWidth)
			{
				matrix[currentMatrix] = (double*)realloc(matrix[currentMatrix], sizeof(double) * newMatrixHeight * newMatrixWidth);
				time_matrix           = (int*)realloc(time_matrix,              sizeof(int)    * newMatrixHeight * newMatrixWidth);
			}
			
			matrixWidth   = newMatrixWidth;
			matrixHeight  = newMatrixHeight;
			currentMatrix = newMatrix;
			
			if(printResults)
			{
				int _;

				if(procJ == 0)
				{
					if(procI > 0)
					{
						comm.Recv(&_, 1, MPI_INTEGER, mpi_rank - 1, 0, NULL);
					}
					else
					{
						printf("after\n");
					}
				}
	
				for(int i = 0; i < matrixHeight; i++)
				{
					if(procJ > 0)
					{
						comm.Recv(&_, 1, MPI_INTEGER, mpi_rank - 1, 0, NULL);
					}
					else if(i > 0)
					{
						comm.Recv(&_, 1, MPI_INTEGER, mpi_rank + bpNumberJ, 0, NULL);
					}

					for(int j = 0; j < matrixWidth; j++)
					{
						printf("%-8lg ", matrix[currentMatrix][i * matrixWidth + j]);
					}
	
					if(procJ < bpNumberJ)
					{
						comm.Send(&_, 1, MPI_INTEGER, mpi_rank + 1, 0);
					}
					else
					{
						printf("\n");
				
						if(i < matrixHeight - 1)
						{
				
							comm.Send(&_, 1, MPI_INTEGER, mpi_rank - bpNumberJ, 0);
						}
					}
				}
	
				if(procI < bpNumberI && procJ == bpNumberJ)
				{
					comm.Send(&_, 1, MPI_INTEGER, mpi_rank + 1, 0);
				}
			}
		}
	}

	free(solutionI[0]);
	free(solutionJ[0]);
	free(solutionI[1]);
	free(solutionJ[1]);
	free(matrix[0]);
	free(matrix[1]);
	free(time_matrix);
}