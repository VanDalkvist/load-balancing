using Core;

namespace CoreParallel
{
    /// <summary>
    /// �������������� �������, ������������ ��������� ���������
    /// </summary>
    public interface ISolutionModification
    {
        /// <summary>
        /// �������� ��� ���������� ������� ����� �������,
        /// ���� ���������� ������� ��������������
        /// </summary>
        /// <param name="solution">�������� �������</param>
        /// <returns>��������������� �������</returns>
        ISolution ApplyTo(ISolution solution);
    }
}