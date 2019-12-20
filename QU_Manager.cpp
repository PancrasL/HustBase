#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"

//单表无条件查询
RC singleNoConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);

//单表条件查询
RC singleConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);

//多表查询
RC multiSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);

//递归获取多表查询结果
RC recurSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res, int curTable, int *offsets, char *curResult);

//判断涉及查询的表是否存在
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
* 第一组参数表示查询涉及的属性
* 第二组表示查询涉及的表
* 第三组表示查询条件
* 最后一个参数res用于返回查询结果集
* 查询优化：优化查询过程，当查询涉及多个表时，设计高效的查询过程
*/
RC Select(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{
	RC rc;

	rc = checkTable(nRelations, relations);
	if (rc != SUCCESS)
	{
		return rc;
	}
	/*分单表查询和多表查询*/
	if (nRelations == 1)
	{ //单表查询
		if (nConditions == 0)
		{  //无条件查询
			rc = singleNoConditionSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
		}
		else
		{	//条件查询
			//rc = singleConditionSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
		}
	}
	else if (nRelations > 1)
	{ //多表条件查询
		//rc = multiSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
	}

	return rc;
}

RC singleNoConditionSelect(int nSelAttrs, RelAttr ** selAttrs, int nRelations, char ** relations, int nConditions, Condition * conditions, SelResult * res)
{
	RC rc;
	//如果某个属性上有索引，则索引查询；否则，全文件扫描
	if (false)
	{ //此处判断索引情况，暂未实现

	}
	else { //无索引，全表扫描
		/*扫描SYSTABLES获得表的属性个数*/
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

		//设置查询结果列数
		memcpy(&(res->col_num), record->pData + sizeof(SysTable::tabName), sizeof(SysTable::attrCount));
		//释放资源
		CloseScan(rm_fileScan);
		RM_CloseFile(rm_table);
		delete rm_table;

		/*扫描SYSCOLUMNS获得属性的偏移量和类型*/
		int offset[20];		//各属性的偏移量
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
			//属性类型
			memcpy(&res->type[i], column + 42, sizeof(AttrType));
			//属性名
			memcpy(&res->fields[i], column + 21, 21);
			//属性偏移量
			memcpy(&offset[i], column + 50, sizeof(int));
			//属性长度
			memcpy(&res->length[i], column + 46, sizeof(int));

			free(record->pData);
		}
		CloseScan(rm_fileScan);
		rc = RM_CloseFile(rm_column);
		delete rm_column;

		/*扫描记录表，找出所有记录*/
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
		SelResult *curRes = res;  //尾插法向链表中插入新结点
		while (GetNextRec(rm_fileScan, record) == SUCCESS)
		{
			if (curRes->row_num >= 100) //每个节点最多记录100条记录
			{ //当前结点已经保存100条记录时，新建结点
				curRes->next_res = new SelResult(*curRes);

				curRes = curRes->next_res;
				curRes->row_num = 0;
				curRes->next_res = NULL;
			}

			auto &resultRecord = curRes->res[curRes->row_num];//结果集的新记录

			int row_num = curRes->row_num;
			int col_num = curRes->col_num;
			resultRecord = new char*[col_num];//该记录保存了每个属性的值，供curRes->col_num个属性

			//将具体的属性值从record->pData中提取出来
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

//判断查询涉及的表是否都存在
//输入参数
//    nRelations: 表的个数
//    relations: 指向表名数组的指针
//当任意一个表不存在时，返回TABLE_NOT_EXIST
//都存在，则返回SUCCESS
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

