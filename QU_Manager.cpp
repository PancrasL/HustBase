#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"

//������������ѯ
RC singleNoConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);

//����������ѯ
RC singleConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);

//����ѯ
RC multiSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);

//�ݹ��ȡ����ѯ���
RC recurSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res, int curTable, int *offsets, char *curResult);

//�ж��漰��ѯ�ı��Ƿ����
RC checkTable(int nRelations, char **relations);

void Init_Result(SelResult * res) {
	res->next_res = NULL;
}

void Destory_Result(SelResult * res) {
	for (int i = 0; i < res->row_num; i++) {
		for (int j = 0; j < res->col_num; j++) {
			delete[] res->res[i][j];
		}
		delete[] res->res[i];
	}
	if (res->next_res != NULL) {
		Destory_Result(res->next_res);
		res->next_res = NULL;
	}

}

RC Query(char * sql, SelResult * res) {
	RC rc;
	sqlstr * sqlType = NULL;
	sqlType = get_sqlstr();
	rc = parse(sql, sqlType);

	if (rc != SUCCESS)
	{
		return rc;
	}

	selects sel = sqlType->sstr.sel;
	rc = Select(sel.nSelAttrs, sel.selAttrs, sel.nRelations, sel.relations, sel.nConditions, sel.conditions, res);

	return rc;
}

/*
* ��һ�������ʾ��ѯ�漰������
* �ڶ����ʾ��ѯ�漰�ı�
* �������ʾ��ѯ����
* ���һ������res���ڷ��ز�ѯ�����
* ��ѯ�Ż����Ż���ѯ���̣�����ѯ�漰�����ʱ����Ƹ�Ч�Ĳ�ѯ����
*/
RC Select(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{
	RC rc;

	rc = checkTable(nRelations, relations);
	if (rc != SUCCESS)
	{
		return rc;
	}
	/*�ֵ����ѯ�Ͷ���ѯ*/
	if (nRelations == 1)
	{ //�����ѯ
		if (nConditions == 0)
		{  //��������ѯ
			rc = singleNoConditionSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
		}
		else
		{	//������ѯ
			//rc = singleConditionSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
		}
	}
	else if (nRelations > 1)
	{ //���������ѯ
		//rc = multiSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
	}

	return rc;
}

RC singleNoConditionSelect(int nSelAttrs, RelAttr ** selAttrs, int nRelations, char ** relations, int nConditions, Condition * conditions, SelResult * res)
{
	RC rc;
	//���ĳ������������������������ѯ������ȫ�ļ�ɨ��
	if (false)
	{ //�˴��ж������������δʵ��

	}
	else { //��������ȫ��ɨ��
		/*ɨ��SYSTABLES��ñ�����Ը���*/
		RM_FileHandle *rm_table = new RM_FileHandle();
		rm_table->bOpen = false;
		rc = RM_OpenFile("SYSTABLES", rm_table);
		if (rc != SUCCESS)
			return rc;

		RM_FileScan *rm_fileScan = new RM_FileScan();
		RM_Record *record = new RM_Record();
		Con con;

		con.attrType = chars;
		con.bLhsIsAttr = 1;
		con.LattrOffset = 0;
		con.LattrLength = strlen(*relations) + 1;
		con.compOp = EQual;
		con.bRhsIsAttr = 0;
		con.Rvalue = *relations;

		rm_fileScan->bOpen = false;
		rc = OpenScan(rm_fileScan, rm_table, 1, &con);
		if (rc != SUCCESS)
			return rc;

		rc = GetNextRec(rm_fileScan, record);
		if (rc != SUCCESS)
			return rc;

		//���ò�ѯ�������
		memcpy(&(res->col_num), record->pData + sizeof(SysTable::tabName), sizeof(SysTable::attrCount));
		//�ͷ���Դ
		CloseScan(rm_fileScan);
		RM_CloseFile(rm_table);
		delete rm_table;

		/*ɨ��SYSCOLUMNS������Ե�ƫ����������*/
		int offset[20];		//�����Ե�ƫ����
		RM_FileHandle *rm_column = new RM_FileHandle();
		rm_column->bOpen = false;
		rc = RM_OpenFile("SYSCOLUMNS", rm_column);
		if (rc != SUCCESS)
			return rc;

		rc = OpenScan(rm_fileScan, rm_column, 1, &con);
		if (rc != SUCCESS)
			return rc;

		for (int i = 0; i < res->col_num; i++)
		{
			rc = GetNextRec(rm_fileScan, record);
			if (rc != SUCCESS)
				return rc;

			char * column = record->pData;
			//��������
			memcpy(&res->type[i], column + 42, sizeof(AttrType));
			//������
			memcpy(&res->fields[i], column + 21, 21);
			//����ƫ����
			memcpy(&offset[i], column + 50, sizeof(int));
			//���Գ���
			memcpy(&res->length[i], column + 46, sizeof(int));

			free(record->pData);
		}
		CloseScan(rm_fileScan);
		rc = RM_CloseFile(rm_column);
		delete rm_column;

		/*ɨ���¼���ҳ����м�¼*/
		RM_FileHandle *rm_data = new RM_FileHandle();
		rm_data->bOpen = false;
		rc = RM_OpenFile(*relations, rm_data);
		if (rc != SUCCESS)
			return rc;

		rc = OpenScan(rm_fileScan, rm_data, 0, NULL);
		if (rc != SUCCESS)
			return rc;

		int i = 0;
		res->row_num = 0;
		SelResult *curRes = res;  //β�巨�������в����½��
		while (GetNextRec(rm_fileScan, record) == SUCCESS)
		{
			if (curRes->row_num >= 100) //ÿ���ڵ�����¼100����¼
			{ //��ǰ����Ѿ�����100����¼ʱ���½����
				curRes->next_res = new SelResult(*curRes);

				curRes = curRes->next_res;
				curRes->row_num = 0;
				curRes->next_res = NULL;
			}

			auto &resultRecord = curRes->res[curRes->row_num];//��������¼�¼

			int row_num = curRes->row_num;
			int col_num = curRes->col_num;
			resultRecord = new char*[col_num];//�ü�¼������ÿ�����Ե�ֵ����curRes->col_num������

			//�����������ֵ��record->pData����ȡ����
			for (int i = 0; i < res->col_num; i++) {
				resultRecord[i] = new char[curRes->length[i]];
				memcpy(resultRecord[i], record->pData + offset[i], curRes->length[i]);
			}
			curRes->row_num++;
			free(record->pData);
		}

		CloseScan(rm_fileScan);
		delete rm_fileScan;
		RM_CloseFile(rm_data);
		delete rm_data;
	}
	return RC();
}

//�жϲ�ѯ�漰�ı��Ƿ񶼴���
//�������
//    nRelations: ��ĸ���
//    relations: ָ����������ָ��
//������һ��������ʱ������TABLE_NOT_EXIST
//�����ڣ��򷵻�SUCCESS
RC checkTable(int nRelations, char **relations)
{
	if (_access("SYSTABLES", 0) == -1 || _access("SYSCOLUMNS", 0) == -1) {
		return DB_NOT_EXIST;
	}
	for (int i = 0; i < nRelations; i++) {
		if (_access(relations[i], 0) == -1)
			return TABLE_NOT_EXIST;
	}
	return SUCCESS;
}

