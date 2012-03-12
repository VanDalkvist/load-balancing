using System.Threading;
using Core;

namespace CoreParallel
{
    /// <summary>
    /// ���������� ��� IPlanner
    /// </summary>
    public static class PlannerExtensions
    {
        /// <summary>
        /// ��������� ���������� ���� ���������� � ����������
        /// </summary>
        /// <typeparam name="T">��� �������� � ������� ������</typeparam>
        /// <typeparam name="TCollectedData">��� ���������� ������</typeparam>
        /// <param name="planner">����������� ��������</param>
        /// <param name="index">������ ������ �������� �������</param>
        /// <param name="collector">������� ����������</param>
        /// <returns>��������� ����������</returns>
        public static TCollectedData RunCollector<T, TCollectedData>(this IPlanner<T> planner, IIndex index, ICollector<T, TCollectedData> collector)
        {
            var e = new ManualResetEvent(false);
            TCollectedData result = default(TCollectedData);
            planner.RunCollectorAsync(index, collector, (data) =>
                {
                    result = data;
                    e.Set();
                });
            e.WaitOne();

            return result;
        }
    }
}