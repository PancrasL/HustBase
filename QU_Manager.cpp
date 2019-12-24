#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"
#include <map>
#include <vector>

using std::map;
using std::string;
using std::vector;
/*�Զ��庯����ṹ*/

//���Ե�Ԫ����
struct AttrMetaData {
	string tableName;
	string attrName;
	AttrType type;
	int length;
	int offset;
	AttrMetaData() {};
	AttrMetaData(string _tableName, string _attrName, AttrType _type, int _length, int _offset)
		:tableName(_tableName), attrName(_attrName), type(_type), length(_length), offset(_offset) {};
};
//table��Ԫ����
struct TableMetaData {
	string tableName;			//����
	vector<AttrMetaData> attrInfo;	//������Ϣ
	map<string, int> attrIndex;	//��������������Ϣ�±��ӳ��
};
//�ж��漰��ѯ�ı��Ƿ����
RC checkTable(int nRelations, char **relations);
//��ȡtable��Ԫ����
RC getTableMetaData(TableMetaData &tableMetaData, char *tableName);
//��ȡĳ��table��ĳ��attribute, ���浽info��
RC getAttrInfo(const string &tableName, const string &attrName, map<string, TableMetaData> & tableMetaDatas, AttrMetaData &info) {
	if (tableMetaDatas.count(tableName) == 0)//������
		return TABLE_NOT_EXIST;

	TableMetaData &tMetaData = tableMetaDatas[tableName];
	if (tMetaData.attrIndex.count(attrName) == 0)//���Բ�����
		return ATTR_NOT_EXIST;

	int aIndex = tMetaData.attrIndex[attrName];
	info = tMetaData.attrInfo[aIndex];

	return SUCCESS;
}
//��ȡ��һ���ѿ�����
bool getNextDkr(vector<RM_FileHandle> &datas, vector<RM_FileScan> &scans, vector<RM_Record> &dkrRecords);
//�Ƚ�ֵ
template<typename T>
bool compare(T a, T b, CompOp op) {
	switch (op)
	{
	case CompOp::EQual:
		return a == b;
		break;
	case CompOp::GEqual:
		return a >= b;
		break;
	case CompOp::GreatT:
		return a > b;
		break;
	case CompOp::LEqual:
		return a <= b;
		break;
	case CompOp::LessT:
		return a < b;
		break;
	case CompOp::NEqual:
		return a != b;
		break;
	default:
		return false;
		break;
	}
}
bool compareValue(const char *left, const char *right, AttrType type, CompOp op) {
	switch (type)
	{
	case AttrType::ints: {
		int a, b;
		memcpy(&a, left, sizeof(int));
		memcpy(&b, right, sizeof(int));
		return compare(a, b, op);
	}
	case AttrType::floats: {
		float a, b;
		memcpy(&a, left, sizeof(int));
		memcpy(&b, right, sizeof(int));
		return compare(a, b, op);
	}
	case AttrType::chars: {
		string a = left;
		string b = right;
		return compare(a, b, op);
	}
	default:
		return false;
	}
}

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
	//�жϲ�ѯ�漰�ı��Ƿ񶼴���
	rc = checkTable(sel.nRelations, sel.relations);
	if (rc != SUCCESS)
		return rc;
	rc = Select(sel.nSelAttrs, sel.selAttrs, sel.nRelations, sel.relations, sel.nConditions, sel.conditions, res);

	return rc;
}

RC Select(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{
	RC rc;

	/*��ȡ�漰�ı��Ԫ����*/
	map<string, TableMetaData> tableMetaDatas;
	map<string, int> tableIndex;//����relations�е��±�
	for (int i = 0; i < nRelations; i++) {
		getTableMetaData(tableMetaDatas[relations[i]], relations[i]);
		tableIndex[relations[i]] = i;
	}

	/*��nRelations�����ݱ�*/
	vector<RM_FileHandle> datas(nRelations);
	for (int i = 0; i < nRelations; i++) {
		datas[i].bOpen = false;
		rc = RM_OpenFile(relations[i], &datas[i]);
		if (rc != SUCCESS)
			return rc;
	}

	/*��ȡselAttrs��Ӧ��������Ϣ*/
	vector<AttrMetaData> selAttrInfo;
	//ȫ���ѯ
	if (nSelAttrs == 1 && selAttrs[0]->relName == NULL && strcmp(selAttrs[0]->attrName, "*") == 0) {
		for (int i = nRelations - 1; i >= 0; i--) {
			selAttrInfo.insert(selAttrInfo.end(),tableMetaDatas[relations[i]].attrInfo.begin(), tableMetaDatas[relations[i]].attrInfo.end());
		}
	}
	//���ֲ�ѯ
	else {
		selAttrInfo.resize(nSelAttrs);
		for (int i = 0; i < nSelAttrs; i++) {
			string relName = selAttrs[nSelAttrs -1 - i]->relName;
			string attrName = selAttrs[nSelAttrs - 1 - i]->attrName;

			rc = getAttrInfo(selAttrs[nSelAttrs - 1 - i]->relName, selAttrs[nSelAttrs - 1 - i]->attrName, tableMetaDatas, selAttrInfo[i]);
			if (rc != SUCCESS)
				return rc;
		}
	}
	

	/*���õѿ����������ѯ���*/
	bool hasEmptyTable = false;
	vector<RM_FileScan> scans(nRelations);
	vector<RM_Record> dkrRecords(nRelations);
	for (int i = 0; i < nRelations; i++) {
		scans[i].bOpen = false;
		rc = OpenScan(&scans[i], &datas[i], 0, NULL);
		if (rc != SUCCESS)
			return rc;
		rc = GetNextRec(&scans[i], &dkrRecords[i]);
		if (rc != SUCCESS) {//���ڿձ�
			hasEmptyTable = true;
			break;
		}
	}

	int rowNum = 0;
	vector<vector<char*> > result;
	if (hasEmptyTable == false) {//�������漰�ı��д��ڿձ����ѯ�����Ϊ��
		do {
			//��������conditions��һ�ж�
			bool isOK = true;
			for (int i = 0; i < nConditions; i++) {
				//��������ֵ
				if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0) {
					AttrMetaData leftAttrInfo;
					rc = getAttrInfo(conditions[i].lhsAttr.relName, conditions[i].lhsAttr.attrName, tableMetaDatas, leftAttrInfo);
					if (rc != SUCCESS)
						return rc;

					if (leftAttrInfo.type != conditions[i].rhsValue.type) {//���Ͳ�ƥ��
						isOK = false;
						break;
					}

					char *left, *right;
					int tIndex = tableIndex[leftAttrInfo.tableName];
					left = dkrRecords[tIndex].pData + leftAttrInfo.offset;
					right = (char *)conditions[i].rhsValue.data;
					if (compareValue(left, right, leftAttrInfo.type, conditions[i].op) == false) {//��¼������ɸѡ����
						isOK = false;
						break;
					}
				}
				//��ֵ������
				else if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1) {
					AttrMetaData rightAttrInfo;
					rc = getAttrInfo(conditions[i].rhsAttr.relName, conditions[i].rhsAttr.attrName, tableMetaDatas, rightAttrInfo);
					if (rc != SUCCESS)
						return rc;

					if (rightAttrInfo.type != conditions[i].lhsValue.type) {//���Ͳ�ƥ��
						isOK = false;
						break;
					}

					char *left, *right;
					int tIndex = tableIndex[rightAttrInfo.tableName];
					right = dkrRecords[tIndex].pData + rightAttrInfo.offset;
					left = (char *)conditions[i].lhsValue.data;
					if (compareValue(left, right, rightAttrInfo.type, conditions[i].op) == false) {//��¼������ɸѡ����
						isOK = false;
						break;
					}
				}
				//���Ҿ�����
				else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1) {
					AttrMetaData leftAttrInfo, rightAttrInfo;

					rc = getAttrInfo(conditions[i].lhsAttr.relName, conditions[i].lhsAttr.attrName, tableMetaDatas, leftAttrInfo);
					if (rc != SUCCESS)
						return rc;
					rc = getAttrInfo(conditions[i].rhsAttr.relName, conditions[i].rhsAttr.attrName, tableMetaDatas, rightAttrInfo);
					if (rc != SUCCESS)
						return rc;

					if (leftAttrInfo.type != rightAttrInfo.type) {//���Ͳ�ƥ��
						isOK = false;
						break;
					}

					char *left, *right;
					int leftTableIndex = tableIndex[leftAttrInfo.tableName];
					int rightTableIndex = tableIndex[rightAttrInfo.tableName];
					left = dkrRecords[leftTableIndex].pData + leftAttrInfo.offset;
					right = dkrRecords[rightTableIndex].pData + rightAttrInfo.offset;
					if (compareValue(left, right, rightAttrInfo.type, conditions[i].op) == false) {//��¼������ɸѡ����
						isOK = false;
						break;
					}
				}
				conditions[i];
			}
			if (isOK) {//Ŀǰ�ļ�¼����������ɸѡ����
				rowNum++;
				vector<char *> newRecord(selAttrInfo.size());
				for (int i = 0; i < selAttrInfo.size(); i++) {
					newRecord[i] = new char[selAttrInfo[i].length];
					int tIndex = tableIndex[selAttrInfo[i].tableName];
					memcpy(newRecord[i], dkrRecords[tIndex].pData + selAttrInfo[i].offset, selAttrInfo[i].length);
				}
				result.push_back(newRecord);
			}
		} while (getNextDkr(datas, scans, dkrRecords) == true);
	}

	/*���ɽ����*/
	//��������Ԫ����
	res->col_num = selAttrInfo.size();
	res->row_num = 0;
	res->next_res = NULL;
	for (int i = 0; i < selAttrInfo.size(); i++) {
		//��������
		res->type[i] = selAttrInfo[i].type;
		//������
		string attrName;
		attrName = selAttrInfo[i].tableName + "->" + selAttrInfo[i].attrName;
		strcpy(res->fields[i], attrName.c_str());
		//���Գ���
		res->length[i] = selAttrInfo[i].length;
		res->offset[i] = selAttrInfo[i].offset;
	}
	//��������ֵ
	SelResult *curRes = res;
	for (int i = 0; i < result.size(); i++) {
		if (curRes->row_num >= 100) {//����100����¼���½�����ڵ�
			curRes->next_res = new SelResult();

			curRes->row_num = 0;
			curRes->next_res = NULL;
		}

		curRes->res[curRes->row_num] = new char*[selAttrInfo.size()];
		for (int j = 0; j < selAttrInfo.size(); j++) {
			curRes->res[curRes->row_num][j] = result[i][j];
		}
		curRes->row_num++;
	}
	return SUCCESS;
}

//������3����ÿ������3����¼�����ʼ��¼���Ϊ 0 0 0
//ÿ����һ�θú�������¼��ű仯Ϊ 0 0 0 �� 0 0 1 �� 0 0 2 ... �� 0 1 1 �� 0 1 2 �� ... �� 3 3 3 �� false
//��¼���ݱ�����dkrRecords��
bool getNextDkr(vector<RM_FileHandle>& datas, vector<RM_FileScan>& scans, vector<RM_Record>& dkrRecords)
{
	RC rc;
	int tableNum = scans.size();
	int k = tableNum - 1;
	while (k >= 0) {
		rc = GetNextRec(&scans[k], &dkrRecords[k]);
		if (rc == SUCCESS) {
			if (k == tableNum - 1) {
				return true;
			}
			for (int t = k + 1; t < tableNum; t++) {
				CloseScan(&scans[t]);
				OpenScan(&scans[t], &datas[t], 0, NULL);
				if (t != tableNum - 1) {
					GetNextRec(&scans[t], &dkrRecords[t]);
				}
			}
			k = tableNum - 1;
		}
		else {
			k--;
		}
	}
	return false;
}

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
	int attrNum;
	memcpy(&attrNum, record.pData + sizeof(SysTable::tabName), sizeof(SysTable::attrCount));

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

	for (int i = 0; i < attrNum; i++)
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
		tableMetaData.attrIndex[attrName] = i;
		tableMetaData.attrInfo.push_back(AttrMetaData(tableName, attrName, type, length, offset));

		delete[] record.pData;
	}
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_column);

	return SUCCESS;
}

