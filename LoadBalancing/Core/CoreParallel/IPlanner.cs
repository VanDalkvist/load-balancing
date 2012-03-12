using Core;

namespace CoreParallel
{
    /// <summary>
    /// ����������� �������� �� ������ ���������� � ����� �������� �������
    /// </summary>
    /// <typeparam name="T">��� �������� � ������� ������</typeparam>
    public interface IPlanner<T>
    {
        /// <summary>
        /// ������� ���������� � ������� � ������ �������� �������
        /// </summary>
        /// <typeparam name="TCollectedData">��� ���������� ������</typeparam>
        /// <param name="index">������ ������ �������� �������</param>
        /// <param name="collector">������� ����������</param>
        /// <returns>��������� ����������</returns>
        TCollectedData RunCollector<TCollectedData>(IIndex index, ICollector<T, TCollectedData> collector);
    }
}