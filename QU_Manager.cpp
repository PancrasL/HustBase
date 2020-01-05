#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"
#include <map>
#include <vector>
#include <string>
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
//�����ѯ
RC singleTableSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);
//����ѯ
RC multiTableSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);
//�ж��漰��ѯ�ı��Ƿ����
RC checkTable(int nRelations, char **relations);
//��ȡtable��Ԫ����
RC getTableMetaData(TableMetaData &tableMetaData, char *tableName);
//��ȡĳ��table��ĳ��attribute, ���浽info��
RC getAttrMetaData(const char *tableName, const char *attrName, const vector<TableMetaData> & tableMetaData, AttrMetaData &info);
//����scanSequence��ȡ��һ���ѿ���������������浽dkrRecords��ָ��ָ��recordVec�е�record
bool getNextDkr(vector<int> &scanSequence, vector<vector<RM_Record>> &recordVec, vector<RM_Record> &dkrRecords);
//�Ƚ�ֵ,����compare���бȽ�
template<typename T>
bool compare(T a, T b, CompOp op);
bool compareValue(const char *left, const char *right, AttrType type, CompOp op);

//����nSelAttrs��selAttrs��ȡ��ѯ��������Ϣ�����浽selAttrVec��
RC getSelAttrs(int nSelAttrs, RelAttr ** selAttrs, const vector<TableMetaData> &tableMetaData, vector<AttrMetaData>& selAttrVec);
//�ж��Ƿ�ѡ������
Con transConditionToCon(const Condition &condition, const vector<TableMetaData> &tableMetaData);


void Init_Result(SelResult * res) {
	res->next_res = NULL;
}

void Destory_Result(SelResult * res) {
	for (int i = 0; i < res->row_num; i++) {
		delete[] res->res[i][0];
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
	if (nRelations == 1) {
		return singleTableSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
	}
	else {
		return multiTableSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
	}
}

RC singleTableSelect(int nSelAttrs, RelAttr ** selAttrs, int nRelations, char ** relations, int nConditions, Condition * conditions, SelResult * res)
{
	RC rc;
	/*��ȡ���Ԫ����*/
	vector<TableMetaData> tableMetaData(1);
	rc = getTableMetaData(tableMetaData[0], relations[0]);
	CHECK_RC(rc);

	/*�򿪱��ļ�*/
	RM_FileHandle rmData;
	rmData.bOpen = false;
	rc = RM_OpenFile(relations[0], &rmData);
	CHECK_RC(rc);

	/*���ɼ�¼ɸѡ����*/
	Con *cons;
	if (nConditions == 0) {//��������ѯ
		cons = NULL;
	}
	else {//������ѯ
		cons = new Con[nConditions];
		for (int i = 0; i < nConditions; i++) {
			cons[i] = transConditionToCon(conditions[i], tableMetaData);
		}
	}

	/*��ȡ��¼*/
	RM_Record tempRecord;
	vector<RM_Record> recordVec;
	RM_FileScan fileScan;
	fileScan.bOpen = false;
	rc = OpenScan(&fileScan, &rmData, nConditions, cons);
	CHECK_RC(rc);
	while (GetNextRec(&fileScan, &tempRecord) == SUCCESS) {
		recordVec.push_back(tempRecord);
	}
	delete[] cons;
	CloseScan(&fileScan);
	RM_CloseFile(&rmData);

	/*ͶӰ*/
	//��ȡѡ������
	vector<AttrMetaData> selAttrVec;
	rc = getSelAttrs(nSelAttrs, selAttrs, tableMetaData, selAttrVec);
	CHECK_RC(rc);
	//��������
	res->col_num = selAttrVec.size();
	res->row_num = 0;
	res->next_res = NULL;
	int offset = 0;
	for (unsigned i = 0; i < selAttrVec.size(); i++) {
		//������
		strcpy_s(res->fields[i], 20, selAttrVec[i].attrName.c_str());
		//���Գ���
		res->length[i] = selAttrVec[i].length;
		//����ƫ��
		res->offset[i] = offset;
		//��������
		res->type[i] = selAttrVec[i].type;

		offset += res->length[i];
	}
	//������¼
	SelResult *curRes = res;
	for (unsigned i = 0; i < recordVec.size(); i++) {
		if (curRes->row_num >= 100) {
			curRes->next_res = new SelResult(*curRes);

			curRes = curRes->next_res;
			curRes->row_num = 0;
			curRes->next_res = NULL;
		}
		curRes->res[curRes->row_num] = new char*;
		*curRes->res[curRes->row_num] = new char[offset];
		for (unsigned j = 0; j < selAttrVec.size(); j++) {
			memcpy(curRes->res[curRes->row_num][0] + res->offset[j], recordVec[i].pData + selAttrVec[j].offset, selAttrVec[j].length);
		}
		curRes->row_num++;
	}

	return SUCCESS;
}

RC multiTableSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res) {
	RC rc;
	/*��ȡ���Ԫ����*/
	vector<TableMetaData> tableMetaData(nRelations);
	for (int i = 0; i < nRelations; i++) {
		rc = getTableMetaData(tableMetaData[i], relations[i]);
		CHECK_RC(rc);
	}

	/*��nRelations�����ݱ�*/
	vector<RM_FileHandle> rmDatas(nRelations);
	map<string, int> tableIndexMap;//�������ͱ���relations�е��±����ӳ��
	for (int i = 0; i < nRelations; i++) {
		rmDatas[i].bOpen = false;
		rc = RM_OpenFile(relations[i], &rmDatas[i]);
		CHECK_RC(rc);

		tableIndexMap[relations[i]] = i;
	}

	/*��ȡͶӰ������*/
	vector<AttrMetaData> selAttrVec;
	rc = getSelAttrs(nSelAttrs, selAttrs, tableMetaData, selAttrVec);
	CHECK_RC(rc);
	res->col_num = selAttrVec.size();
	res->row_num = 0;
	res->next_res = NULL;
	int offset = 0;
	for (unsigned i = 0; i < selAttrVec.size(); i++) {
		//������
		strcpy_s(res->fields[i], 20, selAttrVec[i].attrName.c_str());
		//���Գ���
		res->length[i] = selAttrVec[i].length;
		//����ƫ��
		res->offset[i] = offset;
		//��������
		res->type[i] = selAttrVec[i].type;

		offset += res->length[i];
	}

	/*���ɼ�¼ɸѡ����*/
	/*ע��ֻ������������ֵ����������ֵ�����������������������Ե������������Ϊ����������������ڵѿ�������ʱ�����ﵽ��ѡ�񡢺����ӵĶ���ѯ�Ż�*/
	vector<vector<Con> > consVec(nRelations);//ÿ������Ӧһ��Con
	vector<Condition> joinConditions;//�����������������������Ե�����
	for (int i = 0; i < nConditions; i++) {
		//��������ֵ
		if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0) {
			AttrMetaData attrInfo;
			getAttrMetaData(conditions[i].lhsAttr.relName, conditions[i].lhsAttr.attrName, tableMetaData, attrInfo);

			int tIndex = tableIndexMap[attrInfo.tableName];
			consVec[tIndex].push_back(transConditionToCon(conditions[i], tableMetaData));
		}
		//��ֵ������
		else if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1) {
			AttrMetaData attrInfo;
			getAttrMetaData(conditions[i].rhsAttr.relName, conditions[i].rhsAttr.attrName, tableMetaData, attrInfo);

			int tIndex = tableIndexMap[attrInfo.tableName];
			consVec[tIndex].push_back(transConditionToCon(conditions[i], tableMetaData));
		}
		//������������
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1) {
			joinConditions.push_back(conditions[i]);
		}
		//��ֵ��ֵ
		else {
			return FAIL;
		}
	}

	/*ȡ���漰���ı�ļ�¼*/
	vector<vector<RM_Record>> recordVec(nRelations);
	//��vector��ʽ��Conת��Ϊ������ʽ��Con����Ϊ�����ӿ���������ʽ��Con
	Con **cons;
	cons = new Con*[consVec.size()];
	for (unsigned i = 0; i < consVec.size(); i++) {
		cons[i] = new Con[consVec[i].size()];
		for (unsigned j = 0; j < consVec[i].size(); j++)
			cons[i][j] = consVec[i][j];
	}
	for (int i = 0; i < nRelations; i++) {
		RM_FileScan fileScan;
		fileScan.bOpen = false;
		if (consVec[i].size() == 0) {
			rc = OpenScan(&fileScan, &rmDatas[i], 0, NULL);
		}
		else {
			rc = OpenScan(&fileScan, &rmDatas[i], consVec[i].size(), cons[i]);
		}
		CHECK_RC(rc);
		RM_Record tempRecord;
		while (GetNextRec(&fileScan, &tempRecord) == SUCCESS) {
			recordVec[i].push_back(tempRecord);
		}
		CloseScan(&fileScan);
	}
	for (unsigned i = 0; i < consVec.size(); i++) {
		delete[] cons[i];
	}
	delete[] cons;

	/*�ѿ�����*/
	//��ʼ��ɨ�����Ϊ...0 0 -1,ÿ��������getNextDkr�����һ�����ֿ�ʼ������1����...0 0 -1 > ...0 0 0 > ...0 0 1
	//ÿ����iλ�ﵽ��i�ű�ļ�¼��ʱ����ǰ��һλ
	vector<int> scanSequence(nRelations, 0);
	scanSequence.back() = -1;
	vector<RM_Record> dkrRecords(nRelations);
	SelResult *curRes = res;
	while (getNextDkr(scanSequence, recordVec, dkrRecords)) {
		bool isOK = true;
		for (unsigned i = 0; i < joinConditions.size(); i++) {
			AttrMetaData leftAttr, rightAttr;
			rc = getAttrMetaData(joinConditions[i].lhsAttr.relName, joinConditions[i].lhsAttr.attrName, tableMetaData, leftAttr);
			CHECK_RC(rc);
			rc = getAttrMetaData(joinConditions[i].rhsAttr.relName, joinConditions[i].rhsAttr.attrName, tableMetaData, rightAttr);
			CHECK_RC(rc);

			if (leftAttr.type != rightAttr.type) {
				isOK = false;
				break;
			}
			else {
				RM_Record leftRecord, rightRecord;
				leftRecord = dkrRecords[tableIndexMap[leftAttr.tableName]];
				rightRecord = dkrRecords[tableIndexMap[rightAttr.tableName]];

				char *leftData, *rightData;
				leftData = leftRecord.pData + leftAttr.offset;
				rightData = rightRecord.pData + rightAttr.offset;
				if (compareValue(leftData, rightData, leftAttr.type, joinConditions[i].op) == false) {
					isOK = false;
					break;
				}
			}
		}
	
		if (isOK) {
			if (curRes->row_num >= 100) {
				curRes->next_res = new SelResult(*curRes);

				curRes = curRes->next_res;
				curRes->row_num = 0;
				curRes->next_res = NULL;
			}
			//��������res��ԭ����Ϊ�˼��ݲ��Գ���res����ָ��Ϊ������ơ�
			curRes->res[curRes->row_num] = new char*;
			*curRes->res[curRes->row_num] = new char[offset];
			for (unsigned j = 0; j < selAttrVec.size(); j++) {
				int tIndex = tableIndexMap[selAttrVec[j].tableName];
				memcpy(curRes->res[curRes->row_num][0] + res->offset[j], dkrRecords[tIndex].pData + selAttrVec[j].offset, selAttrVec[j].length);
			}
			curRes->row_num++;
		}
	}

	/*�ͷ���Դ*/
	for (unsigned i = 0; i < rmDatas.size(); i++) {
		rc = RM_CloseFile(&rmDatas[i]);
		CHECK_RC(rc);
	}

	return SUCCESS;
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

RC getAttrMetaData(const char * tableName, const char * attrName, const vector<TableMetaData>& tableMetaData, AttrMetaData & info)
{
	string s_relName, s_attrName;
	s_attrName = attrName;
	if (tableName == NULL) {//����Ϊ��,�������б����������ȷ��attrName������һ����
		for (unsigned i = 0; i < tableMetaData.size(); i++) {
			for (unsigned j = 0; j < tableMetaData[i].attrInfo.size(); j++) {
				if (tableMetaData[i].attrInfo[j].attrName == attrName) {
					info = tableMetaData[i].attrInfo[j];
					return SUCCESS;
				}
			}
		}
		//���Բ�����
		return FAIL;
	}
	else {//������Ϊ��
		s_relName = tableName;
		unsigned i = 0;
		while (i < tableMetaData.size()) {
			if (tableMetaData[i].tableName == s_relName)
				break;
			i++;
		}
		//������
		if (i == tableMetaData.size())
			return TABLE_NOT_EXIST;
		//���Բ�����
		if (tableMetaData[i].attrIndex.count(s_attrName) == 0)
			return FAIL;

		int attrIndex = tableMetaData[i].attrIndex.at(s_attrName);
		info = tableMetaData[i].attrInfo[attrIndex];
	}

	return SUCCESS;
}

RC getSelAttrs(int nSelAttrs, RelAttr ** selAttrs, const vector<TableMetaData> &tableMetaData, vector<AttrMetaData>& selAttrVec)
{
	/*��ȡselAttrs��Ӧ��������Ϣ*/
	RC rc = SUCCESS;
	//ȫ���ѯ
	if (nSelAttrs == 1 && selAttrs[0]->relName == NULL && strcmp(selAttrs[0]->attrName, "*") == 0) {
		for (unsigned i = 0; i < tableMetaData.size(); i++) {
			selAttrVec.insert(selAttrVec.end(), tableMetaData[i].attrInfo.begin(), tableMetaData[i].attrInfo.end());
		}
	}
	//���ֲ�ѯ
	else {
		selAttrVec.resize(nSelAttrs);
		for (int i = 0; i < nSelAttrs; i++) {
			rc = getAttrMetaData(selAttrs[nSelAttrs - 1 - i]->relName, selAttrs[nSelAttrs - 1 - i]->attrName, tableMetaData, selAttrVec[i]);
		}
	}
	return rc;
}

Con transConditionToCon(const Condition & condition, const vector<TableMetaData> &tableMetaData)
{
	//��������ֵ
	Con con;
	if (condition.bLhsIsAttr == 1 && condition.bRhsIsAttr == 0) {
		AttrMetaData attrInfo;
		getAttrMetaData(condition.lhsAttr.relName, condition.lhsAttr.attrName, tableMetaData, attrInfo);

		con.bLhsIsAttr = 1;
		con.bRhsIsAttr = 0;
		con.attrType = attrInfo.type;
		con.LattrLength = attrInfo.length;
		con.LattrOffset = attrInfo.offset;
		con.compOp = condition.op;
		con.Rvalue = condition.rhsValue.data;
	}
	//��ֵ������
	else if (condition.bLhsIsAttr == 0 && condition.bRhsIsAttr == 1) {
		AttrMetaData attrInfo;
		getAttrMetaData(condition.rhsAttr.relName, condition.rhsAttr.attrName, tableMetaData, attrInfo);

		con.bLhsIsAttr = 0;
		con.bRhsIsAttr = 1;
		con.attrType = attrInfo.type;
		con.RattrLength = attrInfo.length;
		con.RattrOffset = attrInfo.offset;
		con.compOp = condition.op;
		con.Lvalue = condition.lhsValue.data;
	}
	//���Ҿ�����
	else if (condition.bLhsIsAttr == 1 && condition.bRhsIsAttr == 1) {
		AttrMetaData LattrInfo, RattrInfo;
		getAttrMetaData(condition.lhsAttr.relName, condition.lhsAttr.attrName, tableMetaData, LattrInfo);
		getAttrMetaData(condition.rhsAttr.relName, condition.rhsAttr.attrName, tableMetaData, RattrInfo);

		con.bLhsIsAttr = 1;
		con.bRhsIsAttr = 1;
		con.attrType = LattrInfo.type;
		con.LattrLength = LattrInfo.length;
		con.LattrOffset = LattrInfo.offset;
		con.RattrLength = RattrInfo.length;
		con.RattrOffset = RattrInfo.offset;
		con.compOp = condition.op;
	}
	return con;
}

//������3����ÿ������3����¼�����ʼ��¼���Ϊ 0 0 0
//ÿ����һ�θú�������¼��ű仯Ϊ 0 0 0 �� 0 0 1 �� 0 0 2 ... �� 0 1 1 �� 0 1 2 �� ... �� 3 3 3 �� false
//��¼���ݱ�����dkrRecords��
bool getNextDkr(vector<int> &scanSequence, vector<vector<RM_Record>> &recordVec, vector<RM_Record> &dkrRecords)
{
	//����Ƿ���ڿռ�¼���ռ�¼�����õѿ�����
	for (unsigned i = 0; i < recordVec.size(); i++) {
		if (recordVec[i].size() == 0)
			return false;
	}

	//��ȡ�ѿ�����
	int tableNum = recordVec.size();//�������
	int k = tableNum - 1;
	while (k >= 0) {
		scanSequence[k]++;
		if (scanSequence[k] < recordVec[k].size()) {
			if (k == tableNum - 1) {
				for (int i = 0; i < tableNum; i++) {
					dkrRecords[i] = recordVec[i][scanSequence[i]];
				}
				return true;
			}
			else {
				for (int reset = k + 1; reset < tableNum; reset++) {
					if (reset != tableNum - 1) {
						scanSequence[reset] = 0;
					}
					else {
						scanSequence[reset] = -1;
					}
				}
				k = tableNum - 1;
			}
		}
		else {
			k--;
		}
	}
	return false;
}

template<typename T>
bool compare(T a, T b, CompOp op)
{
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
bool compareValue(const char * left, const char * right, AttrType type, CompOp op)
{
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

RC getTableMetaData(TableMetaData & tableMetaData, char * tableName)
{
	tableMetaData.tableName = tableName;
	RC rc;
	/*ɨ��SYSTABLES��ñ�����Ը���*/
	RM_FileHandle rm_table;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	CHECK_RC(rc);

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
	CHECK_RC(rc);
	rc = GetNextRec(&rm_fileScan, &record);
	CHECK_RC(rc);

	//������Ը���
	int attrNum;
	memcpy(&attrNum, record.pData + 21, sizeof(int));

	//�ͷ���Դ
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_table);

	/*ɨ��SYSCOLUMNS������Ե�ƫ����������*/
	RM_FileHandle rm_column;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	CHECK_RC(rc);
	rc = OpenScan(&rm_fileScan, &rm_column, 1, &con);
	CHECK_RC(rc);

	for (int i = 0; i < attrNum; i++)
	{
		AttrType type;
		int length;
		int offset;
		char attrName[20];
		rc = GetNextRec(&rm_fileScan, &record);
		CHECK_RC(rc);

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

