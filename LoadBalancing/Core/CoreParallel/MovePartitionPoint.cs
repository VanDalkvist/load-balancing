using System;
using System.Collections.Generic;
using Core;

namespace CoreParallel
{
    /// <summary>
    /// �������������� �������, ��������� ���� �� ����� ��������� �� ����� �������
    /// </summary>
    public class MovePartitionPoint : ISolutionModification
    {
        /// <summary>
        /// �������, ���������� ����� ���������� �������
        /// </summary>
        protected class ModifiedSolution : ISolution
        {
            /// <summary>
            /// �������, ���������� ����� ���������� �������
            /// </summary>
            /// <param name="baseSolution">�������� �������</param>
            /// <param name="dimension">�����������, � ������� �������� �������</param>
            /// <param name="i">����� ��������� ����� ���������</param>
            /// <param name="newPosition">����� ������� ����� ���������</param>
            public ModifiedSolution(ISolution baseSolution, int dimension, int i, int newPosition)
            {
                this.dimension = dimension;
                this.i = i;
                this.newPosition = newPosition;

                BaseSolution = baseSolution;
            }

            public int Dimensions
            {
                get { return BaseSolution.Dimensions; }
            }

            public int Size(int dimension)
            {
                return BaseSolution.Size(dimension);
            }

            private IEnumerable<int> MovePosition(IEnumerable<int> enumerable)
            {
                int pos = 0;

                foreach (int p in enumerable)
                {
                    if (pos == i)
                        yield return newPosition;
                    else
                        yield return p;

                    pos++;
                }
            }

            public IEnumerable<int> this[int dimension]
            {
                get
                {
                    if (dimension != this.dimension)
                    {
                        return BaseSolution[dimension];
                    }

                    return MovePosition(BaseSolution[dimension]);
                }
            }

            private readonly int dimension;
            private readonly int i;
            private readonly int newPosition;
            protected readonly ISolution BaseSolution;
        }

        /// <summary>
        /// �������������� �������, ��������� ���� �� ����� ��������� �� ����� �������
        /// </summary>
        /// <param name="dimension">����� ���������</param>
        /// <param name="i">����� ����� ���������</param>
        /// <param name="newPosition">����� ��������� ����� ���������</param>
        public MovePartitionPoint(int dimension, int i, int newPosition)
        {
            this.dimension = dimension;
            this.i = i;
            this.newPosition = newPosition;
        }

        public ISolution ApplyTo(ISolution solution)
        {
            return new ModifiedSolution(solution, dimension, i, newPosition);
        }

        private readonly int dimension;
        private readonly int i;
        private readonly int newPosition;
    }
}