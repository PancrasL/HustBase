#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"
#include <map>
#include <vector>
using std::map;
using std::string;
using std::vector;
//��SYSTABLES��SYSCOLUMNS�л�ȡ��tableԪ���ݱ��浽�˽ṹ����
struct TableMetaData {
	string tableName;		//����
	int attrNum;			//���Ը���
	vector<string> attrNames;//������
	vector<AttrType> types;	//��������
	vector<int> offsets;		//���Ե�ƫ����
	vector<int> lengths;		//����ֵ�ĳ���
	map<string, int> mp;		//���������±��ӳ��
};
//��ȡtable��Ԫ����
RC getTableMetaData(TableMetaData &tableMetaData, char *tableName);

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

RC getTableMetaData(TableMetaData & tableMetaData, char * tableName)
{
	tableMetaData.tableName = tableName;
	RC rc;
	/*ɨ��SYSTABLES��ñ�����Ը���*/
	RM_FileHandle rm_table;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;

	RM_FileScan rm_fileScan;
	RM_Record record;
	Con con;

	con.attrType = chars;
	con.bLhsIsAttr = 1;
	con.LattrOffset = 0;
	con.LattrLength = strlen(tableName) + 1;
	con.compOp = EQual;
	con.bRhsIsAttr = 0;
	con.Rvalue = tableName;

	rm_fileScan.bOpen = false;
	rc = OpenScan(&rm_fileScan, &rm_table, 1, &con);
	if (rc != SUCCESS)
		return rc;
	rc = GetNextRec(&rm_fileScan, &record);
	if (rc != SUCCESS)
		return rc;

	//������Ը���
	memcpy(&tableMetaData.attrNum, record.pData + sizeof(SysTable::tabName), sizeof(SysTable::attrCount));

	//�ͷ���Դ
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_table);

	/*ɨ��SYSCOLUMNS������Ե�ƫ����������*/
	RM_FileHandle rm_column;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = OpenScan(&rm_fileScan, &rm_column, 1, &con);
	if (rc != SUCCESS)
		return rc;

	for (int i = 0; i < tableMetaData.attrNum; i++)
	{
		AttrType type;
		int length;
		int offset;
		char attrName[20];
		rc = GetNextRec(&rm_fileScan, &record);
		if (rc != SUCCESS)
			return rc;

		char * column = record.pData;
		//��������
		memcpy(&type, column + 42, sizeof(AttrType));
		//������
		memcpy(attrName, column + 21, 20);
		//����ƫ����
		memcpy(&offset, column + 50, sizeof(int));
		//���Գ���
		memcpy(&length, column + 46, sizeof(int));

		//�������Լ�¼
		tableMetaData.types.push_back(type);
		tableMetaData.attrNames.push_back(attrName);
		tableMetaData.offsets.push_back(offset);
		tableMetaData.lengths.push_back(length);
		tableMetaData.mp[attrName] = i;

		delete[] record.pData;
	}
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_column);

	return SUCCESS;
}

RC singleNoConditionSelect(int nSelAttrs, RelAttr ** selAttrs, int nRelations, char ** relations, int nConditions, Condition * conditions, SelResult * res)
{
	RC rc;
	//���ĳ������������������������ѯ������ȫ�ļ�ɨ��
	if (false)
	{ //�˴��ж������������δʵ��

	}
	else { //��������ȫ��ɨ��
		/*��ȡ���Ԫ����*/
		TableMetaData tableMetaData;
		rc = getTableMetaData(tableMetaData, relations[0]);
		if (rc != SUCCESS)
			return rc;

		/*��¼��Ҫ��ѯ������*/
		vector<string> realSelAttrs;//��Ҫ��ѯ������
		if (selAttrs[0]->relName == NULL && strcmp(selAttrs[0]->attrName, "*") == 0) {//ȫ���ѯ����select *
			nSelAttrs = tableMetaData.attrNum;
			realSelAttrs = tableMetaData.attrNames;
		}
		else {//���ֲ�ѯ
			realSelAttrs.resize(nSelAttrs);
			for (int k = 0; k < nSelAttrs; k++) {
				realSelAttrs[k] = selAttrs[nSelAttrs - k - 1]->attrName;
			}
		}

		/*��ʼ�������*/
		res->row_num = 0;
		res->col_num = nSelAttrs;
		res->next_res = NULL;
		for (int k = 0; k < nSelAttrs; k++) {
			int attrIndex = tableMetaData.mp[realSelAttrs[k]];
			//������
			strcpy(res->fields[k], tableMetaData.attrNames[attrIndex].c_str());
			//���Գ���
			res->length[k] = tableMetaData.lengths[attrIndex];
			//��������
			res->type[k] = tableMetaData.types[attrIndex];
		}

		/*ɨ���¼���ҳ����м�¼*/
		RM_Record record;
		RM_FileHandle rm_data;
		rm_data.bOpen = false;
		rc = RM_OpenFile(*relations, &rm_data);
		if (rc != SUCCESS)
			return rc;

		RM_FileScan rm_fileScan;
		rm_fileScan.bOpen = false;
		rc = OpenScan(&rm_fileScan, &rm_data, 0, NULL);
		if (rc != SUCCESS)
			return rc;

		SelResult *curRes = res;  //β�巨�������в����½��
		while (GetNextRec(&rm_fileScan, &record) == SUCCESS)
		{
			if (curRes->row_num >= 100) //ÿ���ڵ�����¼100����¼
			{ //��ǰ����Ѿ�����100����¼ʱ���½����
				curRes->next_res = new SelResult(*curRes);

				curRes = curRes->next_res;
				curRes->row_num = 0;
				curRes->next_res = NULL;
			}

			auto &resultRecord = curRes->res[curRes->row_num];	//�ýṹ�����˲�ѯ�������ÿ����¼�ľ���ֵ������tableӵ������a,b,c��
																//���i����¼������curRes->res[i]�£���i����¼������a������curRes->res[i][0]��
																//��a��intΪ��������a��ֵΪcurRes->res[i][0][0-3],�������4�ֽڵ�buf��

			resultRecord = new char*[nSelAttrs];				
			//�����������ֵ��record->pData����ȡ����
			for (int k = 0; k < nSelAttrs; k++) {
				int attrIndex = tableMetaData.mp[realSelAttrs[k]];
				resultRecord[k] = new char[tableMetaData.lengths[attrIndex]];
				memcpy(resultRecord[k], record.pData + tableMetaData.offsets[attrIndex], tableMetaData.lengths[attrIndex]);
			}
			curRes->row_num++;
			delete[] record.pData;
		}

		/*�ͷ���Դ*/
		CloseScan(&rm_fileScan);
		RM_CloseFile(&rm_data);
	}
	return SUCCESS;
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

