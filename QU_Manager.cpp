#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"
#include <map>
#include <vector>
#include <string>
using std::map;
using std::string;
using std::vector;
/*自定义函数与结构*/

//属性的元数据
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
//table的元数据
struct TableMetaData {
	string tableName;			//表名
	vector<AttrMetaData> attrInfo;	//属性信息
	map<string, int> attrIndex;	//属性名和属性信息下标的映射
};
//单表查询
RC singleTableSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);
//多表查询
RC multiTableSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);
//判断涉及查询的表是否存在
RC checkTable(int nRelations, char **relations);
//获取table的元数据
RC getTableMetaData(TableMetaData &tableMetaData, char *tableName);
//获取某个table的某个attribute, 保存到info中
RC getAttrMetaData(const char *tableName, const char *attrName, const vector<TableMetaData> & tableMetaData, AttrMetaData &info);
//根据scanSequence获取下一个笛卡尔积，将结果保存到dkrRecords，指针指向recordVec中的record
bool getNextDkr(vector<int> &scanSequence, vector<vector<RM_Record>> &recordVec, vector<RM_Record> &dkrRecords);
//比较值,采用compare进行比较
template<typename T>
bool compare(T a, T b, CompOp op);
bool compareValue(const char *left, const char *right, AttrType type, CompOp op);

//根据nSelAttrs和selAttrs获取查询的属性信息，保存到selAttrVec中
RC getSelAttrs(int nSelAttrs, RelAttr ** selAttrs, const vector<TableMetaData> &tableMetaData, vector<AttrMetaData>& selAttrVec);
//判断是否选择条件
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
	//判断查询涉及的表是否都存在
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
	/*获取表的元数据*/
	vector<TableMetaData> tableMetaData(1);
	rc = getTableMetaData(tableMetaData[0], relations[0]);
	CHECK_RC(rc);

	/*打开表文件*/
	RM_FileHandle rmData;
	rmData.bOpen = false;
	rc = RM_OpenFile(relations[0], &rmData);
	CHECK_RC(rc);

	/*生成记录筛选条件*/
	Con *cons;
	if (nConditions == 0) {//无条件查询
		cons = NULL;
	}
	else {//条件查询
		cons = new Con[nConditions];
		for (int i = 0; i < nConditions; i++) {
			cons[i] = transConditionToCon(conditions[i], tableMetaData);
		}
	}

	/*读取记录*/
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

	/*投影*/
	//获取选择属性
	vector<AttrMetaData> selAttrVec;
	rc = getSelAttrs(nSelAttrs, selAttrs, tableMetaData, selAttrVec);
	CHECK_RC(rc);
	//拷贝属性
	res->col_num = selAttrVec.size();
	res->row_num = 0;
	res->next_res = NULL;
	int offset = 0;
	for (unsigned i = 0; i < selAttrVec.size(); i++) {
		//属性名
		strcpy_s(res->fields[i], 20, selAttrVec[i].attrName.c_str());
		//属性长度
		res->length[i] = selAttrVec[i].length;
		//属性偏移
		res->offset[i] = offset;
		//属性类型
		res->type[i] = selAttrVec[i].type;

		offset += res->length[i];
	}
	//拷贝记录
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
	/*获取表的元数据*/
	vector<TableMetaData> tableMetaData(nRelations);
	for (int i = 0; i < nRelations; i++) {
		rc = getTableMetaData(tableMetaData[i], relations[i]);
		CHECK_RC(rc);
	}

	/*打开nRelations个数据表*/
	vector<RM_FileHandle> rmDatas(nRelations);
	map<string, int> tableIndexMap;//将表名和表在relations中的下标进行映射
	for (int i = 0; i < nRelations; i++) {
		rmDatas[i].bOpen = false;
		rc = RM_OpenFile(relations[i], &rmDatas[i]);
		CHECK_RC(rc);

		tableIndexMap[relations[i]] = i;
	}

	/*获取投影的属性*/
	vector<AttrMetaData> selAttrVec;
	rc = getSelAttrs(nSelAttrs, selAttrs, tableMetaData, selAttrVec);
	CHECK_RC(rc);
	res->col_num = selAttrVec.size();
	res->row_num = 0;
	res->next_res = NULL;
	int offset = 0;
	for (unsigned i = 0; i < selAttrVec.size(); i++) {
		//属性名
		strcpy_s(res->fields[i], 20, selAttrVec[i].attrName.c_str());
		//属性长度
		res->length[i] = selAttrVec[i].length;
		//属性偏移
		res->offset[i] = offset;
		//属性类型
		res->type[i] = selAttrVec[i].type;

		offset += res->length[i];
	}

	/*生成记录筛选条件*/
	/*注：只生成左属性右值和右属性左值的条件，对于左属性右属性的情况，将其视为连接运算的条件，在笛卡尔运算时处理，达到先选择、后连接的多表查询优化*/
	vector<vector<Con> > consVec(nRelations);//每个表都对应一组Con
	vector<Condition> joinConditions;//连接条件，即左属性右属性的情形
	for (int i = 0; i < nConditions; i++) {
		//左属性右值
		if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0) {
			AttrMetaData attrInfo;
			getAttrMetaData(conditions[i].lhsAttr.relName, conditions[i].lhsAttr.attrName, tableMetaData, attrInfo);

			int tIndex = tableIndexMap[attrInfo.tableName];
			consVec[tIndex].push_back(transConditionToCon(conditions[i], tableMetaData));
		}
		//左值右属性
		else if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1) {
			AttrMetaData attrInfo;
			getAttrMetaData(conditions[i].rhsAttr.relName, conditions[i].rhsAttr.attrName, tableMetaData, attrInfo);

			int tIndex = tableIndexMap[attrInfo.tableName];
			consVec[tIndex].push_back(transConditionToCon(conditions[i], tableMetaData));
		}
		//左属性右属性
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1) {
			joinConditions.push_back(conditions[i]);
		}
		//左值右值
		else {
			return FAIL;
		}
	}

	/*取出涉及到的表的记录*/
	vector<vector<RM_Record>> recordVec(nRelations);
	//将vector形式的Con转换为数组形式的Con，因为函数接口是数组形式的Con
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

	/*笛卡尔积*/
	//初始化扫描序号为...0 0 -1,每调用依次getNextDkr从最后一个数字开始依次增1例如...0 0 -1 > ...0 0 0 > ...0 0 1
	//每当第i位达到第i张表的记录数时，向前进一位
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
			//这样设置res的原因是为了兼容测试程序，res二重指针为冗余设计。
			curRes->res[curRes->row_num] = new char*;
			*curRes->res[curRes->row_num] = new char[offset];
			for (unsigned j = 0; j < selAttrVec.size(); j++) {
				int tIndex = tableIndexMap[selAttrVec[j].tableName];
				memcpy(curRes->res[curRes->row_num][0] + res->offset[j], dkrRecords[tIndex].pData + selAttrVec[j].offset, selAttrVec[j].length);
			}
			curRes->row_num++;
		}
	}

	/*释放资源*/
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
	if (tableName == NULL) {//表名为空,遍历所有表的所有属性确定attrName属于哪一个表
		for (unsigned i = 0; i < tableMetaData.size(); i++) {
			for (unsigned j = 0; j < tableMetaData[i].attrInfo.size(); j++) {
				if (tableMetaData[i].attrInfo[j].attrName == attrName) {
					info = tableMetaData[i].attrInfo[j];
					return SUCCESS;
				}
			}
		}
		//属性不存在
		return FAIL;
	}
	else {//表名不为空
		s_relName = tableName;
		unsigned i = 0;
		while (i < tableMetaData.size()) {
			if (tableMetaData[i].tableName == s_relName)
				break;
			i++;
		}
		//表不存在
		if (i == tableMetaData.size())
			return TABLE_NOT_EXIST;
		//属性不存在
		if (tableMetaData[i].attrIndex.count(s_attrName) == 0)
			return FAIL;

		int attrIndex = tableMetaData[i].attrIndex.at(s_attrName);
		info = tableMetaData[i].attrInfo[attrIndex];
	}

	return SUCCESS;
}

RC getSelAttrs(int nSelAttrs, RelAttr ** selAttrs, const vector<TableMetaData> &tableMetaData, vector<AttrMetaData>& selAttrVec)
{
	/*获取selAttrs对应的属性信息*/
	RC rc = SUCCESS;
	//全表查询
	if (nSelAttrs == 1 && selAttrs[0]->relName == NULL && strcmp(selAttrs[0]->attrName, "*") == 0) {
		for (unsigned i = 0; i < tableMetaData.size(); i++) {
			selAttrVec.insert(selAttrVec.end(), tableMetaData[i].attrInfo.begin(), tableMetaData[i].attrInfo.end());
		}
	}
	//部分查询
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
	//左属性右值
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
	//左值右属性
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
	//左右均属性
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

//假设有3个表，每个表有3条记录，其初始记录编号为 0 0 0
//每调用一次该函数，记录编号变化为 0 0 0 → 0 0 1 → 0 0 2 ... → 0 1 1 → 0 1 2 → ... → 3 3 3 → false
//记录内容保存在dkrRecords中
bool getNextDkr(vector<int> &scanSequence, vector<vector<RM_Record>> &recordVec, vector<RM_Record> &dkrRecords)
{
	//检查是否存在空记录，空记录不会获得笛卡尔积
	for (unsigned i = 0; i < recordVec.size(); i++) {
		if (recordVec[i].size() == 0)
			return false;
	}

	//获取笛卡尔积
	int tableNum = recordVec.size();//表的数量
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
	/*扫描SYSTABLES获得表的属性个数*/
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

	//获得属性个数
	int attrNum;
	memcpy(&attrNum, record.pData + 21, sizeof(int));

	//释放资源
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_table);

	/*扫描SYSCOLUMNS获得属性的偏移量和类型*/
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
		//属性类型
		memcpy(&type, column + 42, sizeof(AttrType));
		//属性名
		memcpy(attrName, column + 21, 20);
		//属性偏移量
		memcpy(&offset, column + 50, sizeof(int));
		//属性长度
		memcpy(&length, column + 46, sizeof(int));

		//插入属性记录
		tableMetaData.attrIndex[attrName] = i;
		tableMetaData.attrInfo.push_back(AttrMetaData(tableName, attrName, type, length, offset));

		delete[] record.pData;
	}
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_column);

	return SUCCESS;
}

