count_row count_columns 
num_processor_row num_processor_col
array_partitions_rows[count_row + 1]
array_partitions_columns[count_columns + 1]
matrix[count_row][count_columns]

���������
��������� �����, � ������� �������� ������� ���������:
count_row  - ���������� ����� (int)
count_columns - ���������� �������� (int)
num_processor_row - ���������� ����������� �� ������� (int)
num_processor_col - ���������� ����������� �� �������� (int)
array_partitions_rows - ������ ������� ��������� �� �������, �������� num_processor_row + 1 ��������� (������ ������� - 0, ��������� - count_row - 1) (int)
array_partitions_columns - ������ ������� ��������� �� �������, �������� num_processor_col + 1 ��������� (������ ������� - 0, ��������� - count_columns - 1) (int)
matrix[count_row][count_columns] - �������� ������� ��������: � ����� .txt �������� �� ������� (double)